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

static int simpleFunction(int a, int b)
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
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunction, B3CCallPtrTag)),
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
            rare->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunction, B3CCallPtrTag)),
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
                rare->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunction, B3CCallPtrTag)),
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
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunction, B3CCallPtrTag)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

static int functionWithHellaArguments(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p, int q, int r, int s, int t, int u, int v, int w, int x, int y, int z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionWithHellaArguments, B3CCallPtrTag)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    CHECK(compileAndRun<int>(proc) == functionWithHellaArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

static uint64_t functionWithHellaArguments2(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h, uint64_t i, uint64_t j, uint64_t k, uint64_t l, uint64_t m, uint64_t n, uint64_t o, uint64_t p, uint64_t q, uint64_t r, uint64_t s, uint64_t t, uint64_t u, uint64_t v, uint64_t w, uint64_t x, uint64_t y, uint64_t z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionWithHellaArguments2, B3CCallPtrTag)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    auto a = compileAndRun<uint64_t>(proc);
    auto b = functionWithHellaArguments2(limit, limit-1, limit-2, limit-3, limit-4, limit-5, limit-6, limit-7, limit-8, limit-9, limit-10, limit-11, limit-12, limit-13, limit-14, limit-15, limit-16, limit-17, limit-18, limit-19, limit-20, limit-21, limit-22, limit-23, limit-24, limit-25);
    CHECK(a == b);
}

static int functionWithHellaArguments3(int arg0,int arg1,int arg2,int arg3,int arg4,int arg5,int arg6,int arg7,int arg8,int arg9,int arg10,int arg11,int arg12,int arg13,int arg14,int arg15,int arg16,int arg17,int arg18,int arg19,int arg20,int arg21,int arg22,int arg23,int arg24,int arg25,int arg26,int arg27,int arg28,int arg29,int arg30,int arg31,int arg32,int arg33,int arg34,int arg35,int arg36,int arg37,int arg38,int arg39,int arg40,int arg41,int arg42,int arg43,int arg44,int arg45,int arg46,int arg47,int arg48,int arg49,int arg50,int arg51,int arg52,int arg53,int arg54,int arg55,int arg56,int arg57,int arg58,int arg59,int arg60,int arg61,int arg62,int arg63,int arg64,int arg65,int arg66,int arg67,int arg68,int arg69,int arg70,int arg71,int arg72,int arg73,int arg74,int arg75,int arg76,int arg77,int arg78,int arg79,int arg80,int arg81,int arg82,int arg83,int arg84,int arg85,int arg86,int arg87,int arg88,int arg89,int arg90,int arg91,int arg92,int arg93,int arg94,int arg95,int arg96,int arg97,int arg98,int arg99,int arg100,int arg101,int arg102,int arg103,int arg104,int arg105,int arg106,int arg107,int arg108,int arg109,int arg110,int arg111,int arg112,int arg113,int arg114,int arg115,int arg116,int arg117,int arg118,int arg119,int arg120,int arg121,int arg122,int arg123,int arg124,int arg125,int arg126,int arg127,int arg128,int arg129,int arg130,int arg131,int arg132,int arg133,int arg134,int arg135,int arg136,int arg137,int arg138,int arg139,int arg140,int arg141,int arg142,int arg143,int arg144,int arg145,int arg146,int arg147,int arg148,int arg149,int arg150,int arg151,int arg152,int arg153,int arg154,int arg155,int arg156,int arg157,int arg158,int arg159,int arg160,int arg161,int arg162,int arg163,int arg164,int arg165,int arg166,int arg167,int arg168,int arg169,int arg170,int arg171,int arg172,int arg173,int arg174,int arg175,int arg176,int arg177,int arg178,int arg179,int arg180,int arg181,int arg182,int arg183,int arg184,int arg185,int arg186,int arg187,int arg188,int arg189,int arg190,int arg191,int arg192,int arg193,int arg194,int arg195,int arg196,int arg197,int arg198,int arg199,int arg200,int arg201,int arg202,int arg203,int arg204,int arg205,int arg206,int arg207,int arg208,int arg209,int arg210,int arg211,int arg212,int arg213,int arg214,int arg215,int arg216,int arg217,int arg218,int arg219,int arg220,int arg221,int arg222,int arg223,int arg224,int arg225,int arg226,int arg227,int arg228,int arg229,int arg230,int arg231,int arg232,int arg233,int arg234,int arg235,int arg236,int arg237,int arg238,int arg239,int arg240,int arg241,int arg242,int arg243,int arg244,int arg245,int arg246,int arg247,int arg248,int arg249,int arg250,int arg251,int arg252,int arg253,int arg254,int arg255,int arg256,int arg257,int arg258,int arg259,int arg260,int arg261,int arg262,int arg263,int arg264,int arg265,int arg266,int arg267,int arg268,int arg269,int arg270,int arg271,int arg272,int arg273,int arg274,int arg275,int arg276,int arg277,int arg278,int arg279,int arg280,int arg281,int arg282,int arg283,int arg284,int arg285,int arg286,int arg287,int arg288,int arg289,int arg290,int arg291,int arg292,int arg293,int arg294,int arg295,int arg296,int arg297,int arg298,int arg299,int arg300,int arg301,int arg302,int arg303,int arg304,int arg305,int arg306,int arg307,int arg308,int arg309,int arg310,int arg311,int arg312,int arg313,int arg314,int arg315,int arg316,int arg317,int arg318,int arg319,int arg320,int arg321,int arg322,int arg323,int arg324,int arg325,int arg326,int arg327,int arg328,int arg329,int arg330,int arg331,int arg332,int arg333,int arg334,int arg335,int arg336,int arg337,int arg338,int arg339,int arg340,int arg341,int arg342,int arg343,int arg344,int arg345,int arg346,int arg347,int arg348,int arg349,int arg350,int arg351,int arg352,int arg353,int arg354,int arg355,int arg356,int arg357,int arg358,int arg359,int arg360,int arg361,int arg362,int arg363,int arg364,int arg365,int arg366,int arg367,int arg368,int arg369,int arg370,int arg371,int arg372,int arg373,int arg374,int arg375,int arg376,int arg377,int arg378,int arg379,int arg380,int arg381,int arg382,int arg383,int arg384,int arg385,int arg386,int arg387,int arg388,int arg389,int arg390,int arg391,int arg392,int arg393,int arg394,int arg395,int arg396,int arg397,int arg398,int arg399,int arg400,int arg401,int arg402,int arg403,int arg404,int arg405,int arg406,int arg407,int arg408,int arg409,int arg410,int arg411,int arg412,int arg413,int arg414,int arg415,int arg416,int arg417,int arg418,int arg419,int arg420,int arg421,int arg422,int arg423,int arg424,int arg425,int arg426,int arg427,int arg428,int arg429,int arg430,int arg431,int arg432,int arg433,int arg434,int arg435,int arg436,int arg437,int arg438,int arg439,int arg440,int arg441,int arg442,int arg443,int arg444,int arg445,int arg446,int arg447,int arg448,int arg449,int arg450,int arg451,int arg452,int arg453,int arg454,int arg455,int arg456,int arg457,int arg458,int arg459,int arg460,int arg461,int arg462,int arg463,int arg464,int arg465,int arg466,int arg467,int arg468,int arg469,int arg470,int arg471,int arg472,int arg473,int arg474,int arg475,int arg476,int arg477,int arg478,int arg479,int arg480,int arg481,int arg482,int arg483,int arg484,int arg485,int arg486,int arg487,int arg488,int arg489,int arg490,int arg491,int arg492,int arg493,int arg494,int arg495,int arg496,int arg497,int arg498,int arg499,int arg500,int arg501,int arg502,int arg503,int arg504,int arg505,int arg506,int arg507,int arg508,int arg509,int arg510,int arg511,int arg512,int arg513,int arg514,int arg515,int arg516,int arg517,int arg518,int arg519,int arg520,int arg521,int arg522,int arg523,int arg524,int arg525,int arg526,int arg527,int arg528,int arg529,int arg530,int arg531,int arg532,int arg533,int arg534,int arg535,int arg536,int arg537,int arg538,int arg539,int arg540,int arg541,int arg542,int arg543,int arg544,int arg545,int arg546,int arg547,int arg548,int arg549,int arg550,int arg551,int arg552,int arg553,int arg554,int arg555,int arg556,int arg557,int arg558,int arg559,int arg560,int arg561,int arg562,int arg563,int arg564,int arg565,int arg566,int arg567,int arg568,int arg569,int arg570,int arg571,int arg572,int arg573,int arg574,int arg575,int arg576,int arg577,int arg578,int arg579,int arg580,int arg581,int arg582,int arg583,int arg584,int arg585,int arg586,int arg587,int arg588,int arg589,int arg590,int arg591,int arg592,int arg593,int arg594,int arg595,int arg596,int arg597,int arg598,int arg599,int arg600,int arg601,int arg602,int arg603,int arg604,int arg605,int arg606,int arg607,int arg608,int arg609,int arg610,int arg611,int arg612,int arg613,int arg614,int arg615,int arg616,int arg617,int arg618,int arg619,int arg620,int arg621,int arg622,int arg623,int arg624,int arg625,int arg626,int arg627,int arg628,int arg629,int arg630,int arg631,int arg632,int arg633,int arg634,int arg635,int arg636,int arg637,int arg638,int arg639,int arg640,int arg641,int arg642,int arg643,int arg644,int arg645,int arg646,int arg647,int arg648,int arg649,int arg650,int arg651,int arg652,int arg653,int arg654,int arg655,int arg656,int arg657,int arg658,int arg659,int arg660,int arg661,int arg662,int arg663,int arg664,int arg665,int arg666,int arg667,int arg668,int arg669,int arg670,int arg671,int arg672,int arg673,int arg674,int arg675,int arg676,int arg677,int arg678,int arg679,int arg680,int arg681,int arg682,int arg683,int arg684,int arg685,int arg686,int arg687,int arg688,int arg689,int arg690,int arg691,int arg692,int arg693,int arg694,int arg695,int arg696,int arg697,int arg698,int arg699,int arg700,int arg701,int arg702,int arg703,int arg704,int arg705,int arg706,int arg707,int arg708,int arg709,int arg710,int arg711,int arg712,int arg713,int arg714,int arg715,int arg716,int arg717,int arg718,int arg719,int arg720,int arg721,int arg722,int arg723,int arg724,int arg725,int arg726,int arg727,int arg728,int arg729,int arg730,int arg731,int arg732,int arg733,int arg734,int arg735,int arg736,int arg737,int arg738,int arg739,int arg740,int arg741,int arg742,int arg743,int arg744,int arg745,int arg746,int arg747,int arg748,int arg749,int arg750,int arg751,int arg752,int arg753,int arg754,int arg755,int arg756,int arg757,int arg758,int arg759,int arg760,int arg761,int arg762,int arg763,int arg764,int arg765,int arg766,int arg767,int arg768,int arg769,int arg770,int arg771,int arg772,int arg773,int arg774,int arg775,int arg776,int arg777,int arg778,int arg779,int arg780,int arg781,int arg782,int arg783,int arg784,int arg785,int arg786,int arg787,int arg788,int arg789,int arg790,int arg791,int arg792,int arg793,int arg794,int arg795,int arg796,int arg797,int arg798,int arg799,int arg800,int arg801,int arg802,int arg803,int arg804,int arg805,int arg806,int arg807,int arg808,int arg809,int arg810,int arg811,int arg812,int arg813,int arg814,int arg815,int arg816,int arg817,int arg818,int arg819,int arg820,int arg821,int arg822,int arg823,int arg824,int arg825,int arg826,int arg827,int arg828,int arg829,int arg830,int arg831,int arg832,int arg833,int arg834,int arg835,int arg836,int arg837,int arg838,int arg839,int arg840,int arg841,int arg842,int arg843,int arg844,int arg845,int arg846,int arg847,int arg848,int arg849,int arg850,int arg851,int arg852,int arg853,int arg854,int arg855,int arg856,int arg857,int arg858,int arg859,int arg860,int arg861,int arg862,int arg863,int arg864,int arg865,int arg866,int arg867,int arg868,int arg869,int arg870,int arg871,int arg872,int arg873,int arg874,int arg875,int arg876,int arg877,int arg878,int arg879,int arg880,int arg881,int arg882,int arg883,int arg884,int arg885,int arg886,int arg887,int arg888,int arg889,int arg890,int arg891,int arg892,int arg893,int arg894,int arg895,int arg896,int arg897,int arg898,int arg899,int arg900,int arg901,int arg902,int arg903,int arg904,int arg905,int arg906,int arg907,int arg908,int arg909,int arg910,int arg911,int arg912,int arg913,int arg914,int arg915,int arg916,int arg917,int arg918,int arg919,int arg920,int arg921,int arg922,int arg923,int arg924,int arg925,int arg926,int arg927,int arg928,int arg929,int arg930,int arg931,int arg932,int arg933,int arg934,int arg935,int arg936,int arg937,int arg938,int arg939,int arg940,int arg941,int arg942,int arg943,int arg944,int arg945,int arg946,int arg947,int arg948,int arg949,int arg950,int arg951,int arg952,int arg953,int arg954,int arg955,int arg956,int arg957,int arg958,int arg959,int arg960,int arg961,int arg962,int arg963,int arg964,int arg965,int arg966,int arg967,int arg968,int arg969,int arg970,int arg971,int arg972,int arg973,int arg974,int arg975,int arg976,int arg977,int arg978,int arg979,int arg980,int arg981,int arg982,int arg983,int arg984,int arg985,int arg986,int arg987,int arg988,int arg989,int arg990,int arg991,int arg992,int arg993,int arg994,int arg995,int arg996,int arg997,int arg998,int arg999,int arg1000,int arg1001,int arg1002,int arg1003,int arg1004,int arg1005,int arg1006,int arg1007,int arg1008,int arg1009,int arg1010,int arg1011,int arg1012,int arg1013,int arg1014,int arg1015,int arg1016,int arg1017,int arg1018,int arg1019,int arg1020,int arg1021,int arg1022,int arg1023,int arg1024,int arg1025,int arg1026,int arg1027,int arg1028,int arg1029,int arg1030,int arg1031,int arg1032,int arg1033,int arg1034,int arg1035,int arg1036,int arg1037,int arg1038,int arg1039,int arg1040,int arg1041,int arg1042,int arg1043,int arg1044,int arg1045,int arg1046,int arg1047,int arg1048,int arg1049,int arg1050,int arg1051,int arg1052,int arg1053,int arg1054,int arg1055,int arg1056,int arg1057,int arg1058,int arg1059,int arg1060,int arg1061,int arg1062,int arg1063,int arg1064,int arg1065,int arg1066,int arg1067,int arg1068,int arg1069,int arg1070,int arg1071,int arg1072,int arg1073,int arg1074,int arg1075,int arg1076,int arg1077,int arg1078,int arg1079,int arg1080,int arg1081,int arg1082,int arg1083,int arg1084,int arg1085,int arg1086,int arg1087,int arg1088,int arg1089,int arg1090,int arg1091,int arg1092,int arg1093,int arg1094,int arg1095,int arg1096,int arg1097,int arg1098,int arg1099,int arg1100,int arg1101,int arg1102,int arg1103,int arg1104,int arg1105,int arg1106,int arg1107,int arg1108,int arg1109,int arg1110,int arg1111,int arg1112,int arg1113,int arg1114,int arg1115,int arg1116,int arg1117,int arg1118,int arg1119,int arg1120,int arg1121,int arg1122,int arg1123,int arg1124,int arg1125,int arg1126,int arg1127,int arg1128,int arg1129,int arg1130,int arg1131,int arg1132,int arg1133,int arg1134,int arg1135,int arg1136,int arg1137,int arg1138,int arg1139,int arg1140,int arg1141,int arg1142,int arg1143,int arg1144,int arg1145,int arg1146,int arg1147,int arg1148,int arg1149,int arg1150,int arg1151,int arg1152,int arg1153,int arg1154,int arg1155,int arg1156,int arg1157,int arg1158,int arg1159,int arg1160,int arg1161,int arg1162,int arg1163,int arg1164,int arg1165,int arg1166,int arg1167,int arg1168,int arg1169,int arg1170,int arg1171,int arg1172,int arg1173,int arg1174,int arg1175,int arg1176,int arg1177,int arg1178,int arg1179,int arg1180,int arg1181,int arg1182,int arg1183,int arg1184,int arg1185,int arg1186,int arg1187,int arg1188,int arg1189,int arg1190,int arg1191,int arg1192,int arg1193,int arg1194,int arg1195,int arg1196,int arg1197,int arg1198,int arg1199,int arg1200,int arg1201,int arg1202,int arg1203,int arg1204,int arg1205,int arg1206,int arg1207,int arg1208,int arg1209,int arg1210,int arg1211,int arg1212,int arg1213,int arg1214,int arg1215,int arg1216,int arg1217,int arg1218,int arg1219,int arg1220,int arg1221,int arg1222,int arg1223,int arg1224,int arg1225,int arg1226,int arg1227,int arg1228,int arg1229,int arg1230,int arg1231,int arg1232,int arg1233,int arg1234,int arg1235,int arg1236,int arg1237,int arg1238,int arg1239,int arg1240,int arg1241,int arg1242,int arg1243,int arg1244,int arg1245,int arg1246,int arg1247,int arg1248,int arg1249,int arg1250,int arg1251,int arg1252,int arg1253,int arg1254,int arg1255,int arg1256,int arg1257,int arg1258,int arg1259,int arg1260,int arg1261,int arg1262,int arg1263,int arg1264,int arg1265,int arg1266,int arg1267,int arg1268,int arg1269,int arg1270,int arg1271,int arg1272,int arg1273,int arg1274,int arg1275,int arg1276,int arg1277,int arg1278,int arg1279,int arg1280,int arg1281,int arg1282,int arg1283,int arg1284,int arg1285,int arg1286,int arg1287,int arg1288,int arg1289,int arg1290,int arg1291,int arg1292,int arg1293,int arg1294,int arg1295,int arg1296,int arg1297,int arg1298,int arg1299,int arg1300,int arg1301,int arg1302,int arg1303,int arg1304,int arg1305,int arg1306,int arg1307,int arg1308,int arg1309,int arg1310,int arg1311,int arg1312,int arg1313,int arg1314,int arg1315,int arg1316,int arg1317,int arg1318,int arg1319,int arg1320,int arg1321,int arg1322,int arg1323,int arg1324,int arg1325,int arg1326,int arg1327,int arg1328,int arg1329,int arg1330,int arg1331,int arg1332,int arg1333,int arg1334,int arg1335,int arg1336,int arg1337,int arg1338,int arg1339,int arg1340,int arg1341,int arg1342,int arg1343,int arg1344,int arg1345,int arg1346,int arg1347,int arg1348,int arg1349,int arg1350,int arg1351,int arg1352,int arg1353,int arg1354,int arg1355,int arg1356,int arg1357,int arg1358,int arg1359,int arg1360,int arg1361,int arg1362,int arg1363,int arg1364,int arg1365,int arg1366,int arg1367,int arg1368,int arg1369,int arg1370,int arg1371,int arg1372,int arg1373,int arg1374,int arg1375,int arg1376,int arg1377,int arg1378,int arg1379,int arg1380,int arg1381,int arg1382,int arg1383,int arg1384,int arg1385,int arg1386,int arg1387,int arg1388,int arg1389,int arg1390,int arg1391,int arg1392,int arg1393,int arg1394,int arg1395,int arg1396,int arg1397,int arg1398,int arg1399,int arg1400,int arg1401,int arg1402,int arg1403,int arg1404,int arg1405,int arg1406,int arg1407,int arg1408,int arg1409,int arg1410,int arg1411,int arg1412,int arg1413,int arg1414,int arg1415,int arg1416,int arg1417,int arg1418,int arg1419,int arg1420,int arg1421,int arg1422,int arg1423,int arg1424,int arg1425,int arg1426,int arg1427,int arg1428,int arg1429,int arg1430,int arg1431,int arg1432,int arg1433,int arg1434,int arg1435,int arg1436,int arg1437,int arg1438,int arg1439,int arg1440,int arg1441,int arg1442,int arg1443,int arg1444,int arg1445,int arg1446,int arg1447,int arg1448,int arg1449,int arg1450,int arg1451,int arg1452,int arg1453,int arg1454,int arg1455,int arg1456,int arg1457,int arg1458,int arg1459,int arg1460,int arg1461,int arg1462,int arg1463,int arg1464,int arg1465,int arg1466,int arg1467,int arg1468,int arg1469,int arg1470,int arg1471,int arg1472,int arg1473,int arg1474,int arg1475,int arg1476,int arg1477,int arg1478,int arg1479,int arg1480,int arg1481,int arg1482,int arg1483,int arg1484,int arg1485,int arg1486,int arg1487,int arg1488,int arg1489,int arg1490,int arg1491,int arg1492,int arg1493,int arg1494,int arg1495,int arg1496,int arg1497,int arg1498,int arg1499,int arg1500,int arg1501,int arg1502,int arg1503,int arg1504,int arg1505,int arg1506,int arg1507,int arg1508,int arg1509,int arg1510,int arg1511,int arg1512,int arg1513,int arg1514,int arg1515,int arg1516,int arg1517,int arg1518,int arg1519,int arg1520,int arg1521,int arg1522,int arg1523,int arg1524,int arg1525,int arg1526,int arg1527,int arg1528,int arg1529,int arg1530,int arg1531,int arg1532,int arg1533,int arg1534,int arg1535,int arg1536,int arg1537,int arg1538,int arg1539,int arg1540,int arg1541,int arg1542,int arg1543,int arg1544,int arg1545,int arg1546,int arg1547,int arg1548,int arg1549,int arg1550,int arg1551,int arg1552,int arg1553,int arg1554,int arg1555,int arg1556,int arg1557,int arg1558,int arg1559,int arg1560,int arg1561,int arg1562,int arg1563,int arg1564,int arg1565,int arg1566,int arg1567,int arg1568,int arg1569,int arg1570,int arg1571,int arg1572,int arg1573,int arg1574,int arg1575,int arg1576,int arg1577,int arg1578,int arg1579,int arg1580,int arg1581,int arg1582,int arg1583,int arg1584,int arg1585,int arg1586,int arg1587,int arg1588,int arg1589,int arg1590,int arg1591,int arg1592,int arg1593,int arg1594,int arg1595,int arg1596,int arg1597,int arg1598,int arg1599,int arg1600,int arg1601,int arg1602,int arg1603,int arg1604,int arg1605,int arg1606,int arg1607,int arg1608,int arg1609,int arg1610,int arg1611,int arg1612,int arg1613,int arg1614,int arg1615,int arg1616,int arg1617,int arg1618,int arg1619,int arg1620,int arg1621,int arg1622,int arg1623,int arg1624,int arg1625,int arg1626,int arg1627,int arg1628,int arg1629,int arg1630,int arg1631,int arg1632,int arg1633,int arg1634,int arg1635,int arg1636,int arg1637,int arg1638,int arg1639,int arg1640,int arg1641,int arg1642,int arg1643,int arg1644,int arg1645,int arg1646,int arg1647,int arg1648,int arg1649,int arg1650,int arg1651,int arg1652,int arg1653,int arg1654,int arg1655,int arg1656,int arg1657,int arg1658,int arg1659,int arg1660,int arg1661,int arg1662,int arg1663,int arg1664,int arg1665,int arg1666,int arg1667,int arg1668,int arg1669,int arg1670,int arg1671,int arg1672,int arg1673,int arg1674,int arg1675,int arg1676,int arg1677,int arg1678,int arg1679,int arg1680,int arg1681,int arg1682,int arg1683,int arg1684,int arg1685,int arg1686,int arg1687,int arg1688,int arg1689,int arg1690,int arg1691,int arg1692,int arg1693,int arg1694,int arg1695,int arg1696,int arg1697,int arg1698,int arg1699,int arg1700,int arg1701,int arg1702,int arg1703,int arg1704,int arg1705,int arg1706,int arg1707,int arg1708,int arg1709,int arg1710,int arg1711,int arg1712,int arg1713,int arg1714,int arg1715,int arg1716,int arg1717,int arg1718,int arg1719,int arg1720,int arg1721,int arg1722,int arg1723,int arg1724,int arg1725,int arg1726,int arg1727,int arg1728,int arg1729,int arg1730,int arg1731,int arg1732,int arg1733,int arg1734,int arg1735,int arg1736,int arg1737,int arg1738,int arg1739,int arg1740,int arg1741,int arg1742,int arg1743,int arg1744,int arg1745,int arg1746,int arg1747,int arg1748,int arg1749,int arg1750,int arg1751,int arg1752,int arg1753,int arg1754,int arg1755,int arg1756,int arg1757,int arg1758,int arg1759,int arg1760,int arg1761,int arg1762,int arg1763,int arg1764,int arg1765,int arg1766,int arg1767,int arg1768,int arg1769,int arg1770,int arg1771,int arg1772,int arg1773,int arg1774,int arg1775,int arg1776,int arg1777,int arg1778,int arg1779,int arg1780,int arg1781,int arg1782,int arg1783,int arg1784,int arg1785,int arg1786,int arg1787,int arg1788,int arg1789,int arg1790,int arg1791,int arg1792,int arg1793,int arg1794,int arg1795,int arg1796,int arg1797,int arg1798,int arg1799,int arg1800,int arg1801,int arg1802,int arg1803,int arg1804,int arg1805,int arg1806,int arg1807,int arg1808,int arg1809,int arg1810,int arg1811,int arg1812,int arg1813,int arg1814,int arg1815,int arg1816,int arg1817,int arg1818,int arg1819,int arg1820,int arg1821,int arg1822,int arg1823,int arg1824,int arg1825,int arg1826,int arg1827,int arg1828,int arg1829,int arg1830,int arg1831,int arg1832,int arg1833,int arg1834,int arg1835,int arg1836,int arg1837,int arg1838,int arg1839,int arg1840,int arg1841,int arg1842,int arg1843,int arg1844,int arg1845,int arg1846,int arg1847,int arg1848,int arg1849,int arg1850,int arg1851,int arg1852,int arg1853,int arg1854,int arg1855,int arg1856,int arg1857,int arg1858,int arg1859,int arg1860,int arg1861,int arg1862,int arg1863,int arg1864,int arg1865,int arg1866,int arg1867,int arg1868,int arg1869,int arg1870,int arg1871,int arg1872,int arg1873,int arg1874,int arg1875,int arg1876,int arg1877,int arg1878,int arg1879,int arg1880,int arg1881,int arg1882,int arg1883,int arg1884,int arg1885,int arg1886,int arg1887,int arg1888,int arg1889,int arg1890,int arg1891,int arg1892,int arg1893,int arg1894,int arg1895,int arg1896,int arg1897,int arg1898,int arg1899,int arg1900,int arg1901,int arg1902,int arg1903,int arg1904,int arg1905,int arg1906,int arg1907,int arg1908,int arg1909,int arg1910,int arg1911,int arg1912,int arg1913,int arg1914,int arg1915,int arg1916,int arg1917,int arg1918,int arg1919,int arg1920,int arg1921,int arg1922,int arg1923,int arg1924,int arg1925,int arg1926,int arg1927,int arg1928,int arg1929,int arg1930,int arg1931,int arg1932,int arg1933,int arg1934,int arg1935,int arg1936,int arg1937,int arg1938,int arg1939,int arg1940,int arg1941,int arg1942,int arg1943,int arg1944,int arg1945,int arg1946,int arg1947,int arg1948,int arg1949,int arg1950,int arg1951,int arg1952,int arg1953,int arg1954,int arg1955,int arg1956,int arg1957,int arg1958,int arg1959,int arg1960,int arg1961,int arg1962,int arg1963,int arg1964,int arg1965,int arg1966,int arg1967,int arg1968,int arg1969,int arg1970,int arg1971,int arg1972,int arg1973,int arg1974,int arg1975,int arg1976,int arg1977,int arg1978,int arg1979,int arg1980,int arg1981,int arg1982,int arg1983,int arg1984,int arg1985,int arg1986,int arg1987,int arg1988,int arg1989,int arg1990,int arg1991,int arg1992,int arg1993,int arg1994,int arg1995,int arg1996,int arg1997,int arg1998,int arg1999,int arg2000,int arg2001,int arg2002,int arg2003,int arg2004,int arg2005,int arg2006,int arg2007,int arg2008,int arg2009,int arg2010,int arg2011,int arg2012,int arg2013,int arg2014,int arg2015,int arg2016,int arg2017,int arg2018,int arg2019,int arg2020,int arg2021,int arg2022,int arg2023,int arg2024,int arg2025,int arg2026,int arg2027,int arg2028,int arg2029,int arg2030,int arg2031,int arg2032,int arg2033,int arg2034,int arg2035,int arg2036,int arg2037,int arg2038,int arg2039,int arg2040,int arg2041,int arg2042,int arg2043,int arg2044,int arg2045,int arg2046,int arg2047,int arg2048,int arg2049,int arg2050,int arg2051,int arg2052,int arg2053,int arg2054,int arg2055,int arg2056,int arg2057,int arg2058,int arg2059,int arg2060,int arg2061,int arg2062,int arg2063,int arg2064,int arg2065,int arg2066,int arg2067,int arg2068,int arg2069,int arg2070,int arg2071,int arg2072,int arg2073,int arg2074,int arg2075,int arg2076,int arg2077,int arg2078,int arg2079,int arg2080,int arg2081,int arg2082,int arg2083,int arg2084,int arg2085,int arg2086,int arg2087,int arg2088,int arg2089,int arg2090,int arg2091,int arg2092,int arg2093,int arg2094,int arg2095,int arg2096,int arg2097,int arg2098,int arg2099,int arg2100,int arg2101,int arg2102,int arg2103,int arg2104,int arg2105,int arg2106,int arg2107,int arg2108,int arg2109,int arg2110,int arg2111,int arg2112,int arg2113,int arg2114,int arg2115,int arg2116,int arg2117,int arg2118,int arg2119,int arg2120,int arg2121,int arg2122,int arg2123,int arg2124,int arg2125,int arg2126,int arg2127,int arg2128,int arg2129,int arg2130,int arg2131,int arg2132,int arg2133,int arg2134,int arg2135,int arg2136,int arg2137,int arg2138,int arg2139,int arg2140,int arg2141,int arg2142,int arg2143,int arg2144,int arg2145,int arg2146,int arg2147,int arg2148,int arg2149,int arg2150,int arg2151,int arg2152,int arg2153,int arg2154,int arg2155,int arg2156,int arg2157,int arg2158,int arg2159,int arg2160,int arg2161,int arg2162,int arg2163,int arg2164,int arg2165,int arg2166,int arg2167,int arg2168,int arg2169,int arg2170,int arg2171,int arg2172,int arg2173,int arg2174,int arg2175,int arg2176,int arg2177,int arg2178,int arg2179,int arg2180,int arg2181,int arg2182,int arg2183,int arg2184,int arg2185,int arg2186,int arg2187,int arg2188,int arg2189,int arg2190,int arg2191,int arg2192,int arg2193,int arg2194,int arg2195,int arg2196,int arg2197,int arg2198,int arg2199,int arg2200,int arg2201,int arg2202,int arg2203,int arg2204,int arg2205,int arg2206,int arg2207,int arg2208,int arg2209,int arg2210,int arg2211,int arg2212,int arg2213,int arg2214,int arg2215,int arg2216,int arg2217,int arg2218,int arg2219,int arg2220,int arg2221,int arg2222,int arg2223,int arg2224,int arg2225,int arg2226,int arg2227,int arg2228,int arg2229,int arg2230,int arg2231,int arg2232,int arg2233,int arg2234,int arg2235,int arg2236,int arg2237,int arg2238,int arg2239,int arg2240,int arg2241,int arg2242,int arg2243,int arg2244,int arg2245,int arg2246,int arg2247,int arg2248,int arg2249,int arg2250,int arg2251,int arg2252,int arg2253,int arg2254,int arg2255,int arg2256,int arg2257,int arg2258,int arg2259,int arg2260,int arg2261,int arg2262,int arg2263,int arg2264,int arg2265,int arg2266,int arg2267,int arg2268,int arg2269,int arg2270,int arg2271,int arg2272,int arg2273,int arg2274,int arg2275,int arg2276,int arg2277,int arg2278,int arg2279,int arg2280,int arg2281,int arg2282,int arg2283,int arg2284,int arg2285,int arg2286,int arg2287,int arg2288,int arg2289,int arg2290,int arg2291,int arg2292,int arg2293,int arg2294,int arg2295,int arg2296,int arg2297,int arg2298,int arg2299,int arg2300,int arg2301,int arg2302,int arg2303,int arg2304,int arg2305,int arg2306,int arg2307,int arg2308,int arg2309,int arg2310,int arg2311,int arg2312,int arg2313,int arg2314,int arg2315,int arg2316,int arg2317,int arg2318,int arg2319,int arg2320,int arg2321,int arg2322,int arg2323,int arg2324,int arg2325,int arg2326,int arg2327,int arg2328,int arg2329,int arg2330,int arg2331,int arg2332,int arg2333,int arg2334,int arg2335,int arg2336,int arg2337,int arg2338,int arg2339,int arg2340,int arg2341,int arg2342,int arg2343,int arg2344,int arg2345,int arg2346,int arg2347,int arg2348,int arg2349,int arg2350,int arg2351,int arg2352,int arg2353,int arg2354,int arg2355,int arg2356,int arg2357,int arg2358,int arg2359,int arg2360,int arg2361,int arg2362,int arg2363,int arg2364,int arg2365,int arg2366,int arg2367,int arg2368,int arg2369,int arg2370,int arg2371,int arg2372,int arg2373,int arg2374,int arg2375,int arg2376,int arg2377,int arg2378,int arg2379,int arg2380,int arg2381,int arg2382,int arg2383,int arg2384,int arg2385,int arg2386,int arg2387,int arg2388,int arg2389,int arg2390,int arg2391,int arg2392,int arg2393,int arg2394,int arg2395,int arg2396,int arg2397,int arg2398,int arg2399,int arg2400,int arg2401,int arg2402,int arg2403,int arg2404,int arg2405,int arg2406,int arg2407,int arg2408,int arg2409,int arg2410,int arg2411,int arg2412,int arg2413,int arg2414,int arg2415,int arg2416,int arg2417,int arg2418,int arg2419,int arg2420,int arg2421,int arg2422,int arg2423,int arg2424,int arg2425,int arg2426,int arg2427,int arg2428,int arg2429,int arg2430,int arg2431,int arg2432,int arg2433,int arg2434,int arg2435,int arg2436,int arg2437,int arg2438,int arg2439,int arg2440,int arg2441,int arg2442,int arg2443,int arg2444,int arg2445,int arg2446,int arg2447,int arg2448,int arg2449,int arg2450,int arg2451,int arg2452,int arg2453,int arg2454,int arg2455,int arg2456,int arg2457,int arg2458,int arg2459,int arg2460,int arg2461,int arg2462,int arg2463,int arg2464,int arg2465,int arg2466,int arg2467,int arg2468,int arg2469,int arg2470,int arg2471,int arg2472,int arg2473,int arg2474,int arg2475,int arg2476,int arg2477,int arg2478,int arg2479,int arg2480,int arg2481,int arg2482,int arg2483,int arg2484,int arg2485,int arg2486,int arg2487,int arg2488,int arg2489,int arg2490,int arg2491,int arg2492,int arg2493,int arg2494,int arg2495,int arg2496,int arg2497,int arg2498,int arg2499,int arg2500,int arg2501,int arg2502,int arg2503,int arg2504,int arg2505,int arg2506,int arg2507,int arg2508,int arg2509,int arg2510,int arg2511,int arg2512,int arg2513,int arg2514,int arg2515,int arg2516,int arg2517,int arg2518,int arg2519,int arg2520,int arg2521,int arg2522,int arg2523,int arg2524,int arg2525,int arg2526,int arg2527,int arg2528,int arg2529,int arg2530,int arg2531,int arg2532,int arg2533,int arg2534,int arg2535,int arg2536,int arg2537,int arg2538,int arg2539,int arg2540,int arg2541,int arg2542,int arg2543,int arg2544,int arg2545,int arg2546,int arg2547,int arg2548,int arg2549,int arg2550,int arg2551,int arg2552,int arg2553,int arg2554,int arg2555,int arg2556,int arg2557,int arg2558,int arg2559,int arg2560,int arg2561,int arg2562,int arg2563,int arg2564,int arg2565,int arg2566,int arg2567,int arg2568,int arg2569,int arg2570,int arg2571,int arg2572,int arg2573,int arg2574,int arg2575,int arg2576,int arg2577,int arg2578,int arg2579,int arg2580,int arg2581,int arg2582,int arg2583,int arg2584,int arg2585,int arg2586,int arg2587,int arg2588,int arg2589,int arg2590,int arg2591,int arg2592,int arg2593,int arg2594,int arg2595,int arg2596,int arg2597,int arg2598,int arg2599,int arg2600,int arg2601,int arg2602,int arg2603,int arg2604,int arg2605,int arg2606,int arg2607,int arg2608,int arg2609,int arg2610,int arg2611,int arg2612,int arg2613,int arg2614,int arg2615,int arg2616,int arg2617,int arg2618,int arg2619,int arg2620,int arg2621,int arg2622,int arg2623,int arg2624,int arg2625,int arg2626,int arg2627,int arg2628,int arg2629,int arg2630,int arg2631,int arg2632,int arg2633,int arg2634,int arg2635,int arg2636,int arg2637,int arg2638,int arg2639,int arg2640,int arg2641,int arg2642,int arg2643,int arg2644,int arg2645,int arg2646,int arg2647,int arg2648,int arg2649,int arg2650,int arg2651,int arg2652,int arg2653,int arg2654,int arg2655,int arg2656,int arg2657,int arg2658,int arg2659,int arg2660,int arg2661,int arg2662,int arg2663,int arg2664,int arg2665,int arg2666,int arg2667,int arg2668,int arg2669,int arg2670,int arg2671,int arg2672,int arg2673,int arg2674,int arg2675,int arg2676,int arg2677,int arg2678,int arg2679,int arg2680,int arg2681,int arg2682,int arg2683,int arg2684,int arg2685,int arg2686,int arg2687,int arg2688,int arg2689,int arg2690,int arg2691,int arg2692,int arg2693,int arg2694,int arg2695,int arg2696,int arg2697,int arg2698,int arg2699,int arg2700,int arg2701,int arg2702,int arg2703,int arg2704,int arg2705,int arg2706,int arg2707,int arg2708,int arg2709,int arg2710,int arg2711,int arg2712,int arg2713,int arg2714,int arg2715,int arg2716,int arg2717,int arg2718,int arg2719,int arg2720,int arg2721,int arg2722,int arg2723,int arg2724,int arg2725,int arg2726,int arg2727,int arg2728,int arg2729,int arg2730,int arg2731,int arg2732,int arg2733,int arg2734,int arg2735,int arg2736,int arg2737,int arg2738,int arg2739,int arg2740,int arg2741,int arg2742,int arg2743,int arg2744,int arg2745,int arg2746,int arg2747,int arg2748,int arg2749,int arg2750,int arg2751,int arg2752,int arg2753,int arg2754,int arg2755,int arg2756,int arg2757,int arg2758,int arg2759,int arg2760,int arg2761,int arg2762,int arg2763,int arg2764,int arg2765,int arg2766,int arg2767,int arg2768,int arg2769,int arg2770,int arg2771,int arg2772,int arg2773,int arg2774,int arg2775,int arg2776,int arg2777,int arg2778,int arg2779,int arg2780,int arg2781,int arg2782,int arg2783,int arg2784,int arg2785,int arg2786,int arg2787,int arg2788,int arg2789,int arg2790,int arg2791,int arg2792,int arg2793,int arg2794,int arg2795,int arg2796,int arg2797,int arg2798,int arg2799,int arg2800,int arg2801,int arg2802,int arg2803,int arg2804,int arg2805,int arg2806,int arg2807,int arg2808,int arg2809,int arg2810,int arg2811,int arg2812,int arg2813,int arg2814,int arg2815,int arg2816,int arg2817,int arg2818,int arg2819,int arg2820,int arg2821,int arg2822,int arg2823,int arg2824,int arg2825,int arg2826,int arg2827,int arg2828,int arg2829,int arg2830,int arg2831,int arg2832,int arg2833,int arg2834,int arg2835,int arg2836,int arg2837,int arg2838,int arg2839,int arg2840,int arg2841,int arg2842,int arg2843,int arg2844,int arg2845,int arg2846,int arg2847,int arg2848,int arg2849,int arg2850,int arg2851,int arg2852,int arg2853,int arg2854,int arg2855,int arg2856,int arg2857,int arg2858,int arg2859,int arg2860,int arg2861,int arg2862,int arg2863,int arg2864,int arg2865,int arg2866,int arg2867,int arg2868,int arg2869,int arg2870,int arg2871,int arg2872,int arg2873,int arg2874,int arg2875,int arg2876,int arg2877,int arg2878,int arg2879,int arg2880,int arg2881,int arg2882,int arg2883,int arg2884,int arg2885,int arg2886,int arg2887,int arg2888,int arg2889,int arg2890,int arg2891,int arg2892,int arg2893,int arg2894,int arg2895,int arg2896,int arg2897,int arg2898,int arg2899,int arg2900,int arg2901,int arg2902,int arg2903,int arg2904,int arg2905,int arg2906,int arg2907,int arg2908,int arg2909,int arg2910,int arg2911,int arg2912,int arg2913,int arg2914,int arg2915,int arg2916,int arg2917,int arg2918,int arg2919,int arg2920,int arg2921,int arg2922,int arg2923,int arg2924,int arg2925,int arg2926,int arg2927,int arg2928,int arg2929,int arg2930,int arg2931,int arg2932,int arg2933,int arg2934,int arg2935,int arg2936,int arg2937,int arg2938,int arg2939,int arg2940,int arg2941,int arg2942,int arg2943,int arg2944,int arg2945,int arg2946,int arg2947,int arg2948,int arg2949,int arg2950,int arg2951,int arg2952,int arg2953,int arg2954,int arg2955,int arg2956,int arg2957,int arg2958,int arg2959,int arg2960,int arg2961,int arg2962,int arg2963,int arg2964,int arg2965,int arg2966,int arg2967,int arg2968,int arg2969,int arg2970,int arg2971,int arg2972,int arg2973,int arg2974,int arg2975,int arg2976,int arg2977,int arg2978,int arg2979,int arg2980,int arg2981,int arg2982,int arg2983,int arg2984,int arg2985,int arg2986,int arg2987,int arg2988,int arg2989,int arg2990,int arg2991,int arg2992,int arg2993,int arg2994,int arg2995,int arg2996,int arg2997,int arg2998,int arg2999,int arg3000,int arg3001,int arg3002,int arg3003,int arg3004,int arg3005,int arg3006,int arg3007,int arg3008,int arg3009,int arg3010,int arg3011,int arg3012,int arg3013,int arg3014,int arg3015,int arg3016,int arg3017,int arg3018,int arg3019,int arg3020,int arg3021,int arg3022,int arg3023,int arg3024,int arg3025,int arg3026,int arg3027,int arg3028,int arg3029,int arg3030,int arg3031,int arg3032,int arg3033,int arg3034,int arg3035,int arg3036,int arg3037,int arg3038,int arg3039,int arg3040,int arg3041,int arg3042,int arg3043,int arg3044,int arg3045,int arg3046,int arg3047,int arg3048,int arg3049,int arg3050,int arg3051,int arg3052,int arg3053,int arg3054,int arg3055,int arg3056,int arg3057,int arg3058,int arg3059,int arg3060,int arg3061,int arg3062,int arg3063,int arg3064,int arg3065,int arg3066,int arg3067,int arg3068,int arg3069,int arg3070,int arg3071,int arg3072,int arg3073,int arg3074,int arg3075,int arg3076,int arg3077,int arg3078,int arg3079,int arg3080,int arg3081,int arg3082,int arg3083,int arg3084,int arg3085,int arg3086,int arg3087,int arg3088,int arg3089,int arg3090,int arg3091,int arg3092,int arg3093,int arg3094,int arg3095,int arg3096,int arg3097,int arg3098,int arg3099,int arg3100,int arg3101,int arg3102,int arg3103,int arg3104,int arg3105,int arg3106,int arg3107,int arg3108,int arg3109,int arg3110,int arg3111,int arg3112,int arg3113,int arg3114,int arg3115,int arg3116,int arg3117,int arg3118,int arg3119,int arg3120,int arg3121,int arg3122,int arg3123,int arg3124,int arg3125,int arg3126,int arg3127,int arg3128,int arg3129,int arg3130,int arg3131,int arg3132,int arg3133,int arg3134,int arg3135,int arg3136,int arg3137,int arg3138,int arg3139,int arg3140,int arg3141,int arg3142,int arg3143,int arg3144,int arg3145,int arg3146,int arg3147,int arg3148,int arg3149,int arg3150,int arg3151,int arg3152,int arg3153,int arg3154,int arg3155,int arg3156,int arg3157,int arg3158,int arg3159,int arg3160,int arg3161,int arg3162,int arg3163,int arg3164,int arg3165,int arg3166,int arg3167,int arg3168,int arg3169,int arg3170,int arg3171,int arg3172,int arg3173,int arg3174,int arg3175,int arg3176,int arg3177,int arg3178,int arg3179,int arg3180,int arg3181,int arg3182,int arg3183,int arg3184,int arg3185,int arg3186,int arg3187,int arg3188,int arg3189,int arg3190,int arg3191,int arg3192,int arg3193,int arg3194,int arg3195,int arg3196,int arg3197,int arg3198,int arg3199,int arg3200,int arg3201,int arg3202,int arg3203,int arg3204,int arg3205,int arg3206,int arg3207,int arg3208,int arg3209,int arg3210,int arg3211,int arg3212,int arg3213,int arg3214,int arg3215,int arg3216,int arg3217,int arg3218,int arg3219,int arg3220,int arg3221,int arg3222,int arg3223,int arg3224,int arg3225,int arg3226,int arg3227,int arg3228,int arg3229,int arg3230,int arg3231,int arg3232,int arg3233,int arg3234,int arg3235,int arg3236,int arg3237,int arg3238,int arg3239,int arg3240,int arg3241,int arg3242,int arg3243,int arg3244,int arg3245,int arg3246,int arg3247,int arg3248,int arg3249,int arg3250,int arg3251,int arg3252,int arg3253,int arg3254,int arg3255,int arg3256,int arg3257,int arg3258,int arg3259,int arg3260,int arg3261,int arg3262,int arg3263,int arg3264,int arg3265,int arg3266,int arg3267,int arg3268,int arg3269,int arg3270,int arg3271,int arg3272,int arg3273,int arg3274,int arg3275,int arg3276,int arg3277,int arg3278,int arg3279,int arg3280,int arg3281,int arg3282,int arg3283,int arg3284,int arg3285,int arg3286,int arg3287,int arg3288,int arg3289,int arg3290,int arg3291,int arg3292,int arg3293,int arg3294,int arg3295,int arg3296,int arg3297,int arg3298,int arg3299,int arg3300,int arg3301,int arg3302,int arg3303,int arg3304,int arg3305,int arg3306,int arg3307,int arg3308,int arg3309,int arg3310,int arg3311,int arg3312,int arg3313,int arg3314,int arg3315,int arg3316,int arg3317,int arg3318,int arg3319,int arg3320,int arg3321,int arg3322,int arg3323,int arg3324,int arg3325,int arg3326,int arg3327,int arg3328,int arg3329,int arg3330,int arg3331,int arg3332,int arg3333,int arg3334,int arg3335,int arg3336,int arg3337,int arg3338,int arg3339,int arg3340,int arg3341,int arg3342,int arg3343,int arg3344,int arg3345,int arg3346,int arg3347,int arg3348,int arg3349,int arg3350,int arg3351,int arg3352,int arg3353,int arg3354,int arg3355,int arg3356,int arg3357,int arg3358,int arg3359,int arg3360,int arg3361,int arg3362,int arg3363,int arg3364,int arg3365,int arg3366,int arg3367,int arg3368,int arg3369,int arg3370,int arg3371,int arg3372,int arg3373,int arg3374,int arg3375,int arg3376,int arg3377,int arg3378,int arg3379,int arg3380,int arg3381,int arg3382,int arg3383,int arg3384,int arg3385,int arg3386,int arg3387,int arg3388,int arg3389,int arg3390,int arg3391,int arg3392,int arg3393,int arg3394,int arg3395,int arg3396,int arg3397,int arg3398,int arg3399,int arg3400,int arg3401,int arg3402,int arg3403,int arg3404,int arg3405,int arg3406,int arg3407,int arg3408,int arg3409,int arg3410,int arg3411,int arg3412,int arg3413,int arg3414,int arg3415,int arg3416,int arg3417,int arg3418,int arg3419,int arg3420,int arg3421,int arg3422,int arg3423,int arg3424,int arg3425,int arg3426,int arg3427,int arg3428,int arg3429,int arg3430,int arg3431,int arg3432,int arg3433,int arg3434,int arg3435,int arg3436,int arg3437,int arg3438,int arg3439,int arg3440,int arg3441,int arg3442,int arg3443,int arg3444,int arg3445,int arg3446,int arg3447,int arg3448,int arg3449,int arg3450,int arg3451,int arg3452,int arg3453,int arg3454,int arg3455,int arg3456,int arg3457,int arg3458,int arg3459,int arg3460,int arg3461,int arg3462,int arg3463,int arg3464,int arg3465,int arg3466,int arg3467,int arg3468,int arg3469,int arg3470,int arg3471,int arg3472,int arg3473,int arg3474,int arg3475,int arg3476,int arg3477,int arg3478,int arg3479,int arg3480,int arg3481,int arg3482,int arg3483,int arg3484,int arg3485,int arg3486,int arg3487,int arg3488,int arg3489,int arg3490,int arg3491,int arg3492,int arg3493,int arg3494,int arg3495,int arg3496,int arg3497,int arg3498,int arg3499,int arg3500,int arg3501,int arg3502,int arg3503,int arg3504,int arg3505,int arg3506,int arg3507,int arg3508,int arg3509,int arg3510,int arg3511,int arg3512,int arg3513,int arg3514,int arg3515,int arg3516,int arg3517,int arg3518,int arg3519,int arg3520,int arg3521,int arg3522,int arg3523,int arg3524,int arg3525,int arg3526,int arg3527,int arg3528,int arg3529,int arg3530,int arg3531,int arg3532,int arg3533,int arg3534,int arg3535,int arg3536,int arg3537,int arg3538,int arg3539,int arg3540,int arg3541,int arg3542,int arg3543,int arg3544,int arg3545,int arg3546,int arg3547,int arg3548,int arg3549,int arg3550,int arg3551,int arg3552,int arg3553,int arg3554,int arg3555,int arg3556,int arg3557,int arg3558,int arg3559,int arg3560,int arg3561,int arg3562,int arg3563,int arg3564,int arg3565,int arg3566,int arg3567,int arg3568,int arg3569,int arg3570,int arg3571,int arg3572,int arg3573,int arg3574,int arg3575,int arg3576,int arg3577,int arg3578,int arg3579,int arg3580,int arg3581,int arg3582,int arg3583,int arg3584,int arg3585,int arg3586,int arg3587,int arg3588,int arg3589,int arg3590,int arg3591,int arg3592,int arg3593,int arg3594,int arg3595,int arg3596,int arg3597,int arg3598,int arg3599,int arg3600,int arg3601,int arg3602,int arg3603,int arg3604,int arg3605,int arg3606,int arg3607,int arg3608,int arg3609,int arg3610,int arg3611,int arg3612,int arg3613,int arg3614,int arg3615,int arg3616,int arg3617,int arg3618,int arg3619,int arg3620,int arg3621,int arg3622,int arg3623,int arg3624,int arg3625,int arg3626,int arg3627,int arg3628,int arg3629,int arg3630,int arg3631,int arg3632,int arg3633,int arg3634,int arg3635,int arg3636,int arg3637,int arg3638,int arg3639,int arg3640,int arg3641,int arg3642,int arg3643,int arg3644,int arg3645,int arg3646,int arg3647,int arg3648,int arg3649,int arg3650,int arg3651,int arg3652,int arg3653,int arg3654,int arg3655,int arg3656,int arg3657,int arg3658,int arg3659,int arg3660,int arg3661,int arg3662,int arg3663,int arg3664,int arg3665,int arg3666,int arg3667,int arg3668,int arg3669,int arg3670,int arg3671,int arg3672,int arg3673,int arg3674,int arg3675,int arg3676,int arg3677,int arg3678,int arg3679,int arg3680,int arg3681,int arg3682,int arg3683,int arg3684,int arg3685,int arg3686,int arg3687,int arg3688,int arg3689,int arg3690,int arg3691,int arg3692,int arg3693,int arg3694,int arg3695,int arg3696,int arg3697,int arg3698,int arg3699,int arg3700,int arg3701,int arg3702,int arg3703,int arg3704,int arg3705,int arg3706,int arg3707,int arg3708,int arg3709,int arg3710,int arg3711,int arg3712,int arg3713,int arg3714,int arg3715,int arg3716,int arg3717,int arg3718,int arg3719,int arg3720,int arg3721,int arg3722,int arg3723,int arg3724,int arg3725,int arg3726,int arg3727,int arg3728,int arg3729,int arg3730,int arg3731,int arg3732,int arg3733,int arg3734,int arg3735,int arg3736,int arg3737,int arg3738,int arg3739,int arg3740,int arg3741,int arg3742,int arg3743,int arg3744,int arg3745,int arg3746,int arg3747,int arg3748,int arg3749,int arg3750,int arg3751,int arg3752,int arg3753,int arg3754,int arg3755,int arg3756,int arg3757,int arg3758,int arg3759,int arg3760,int arg3761,int arg3762,int arg3763,int arg3764,int arg3765,int arg3766,int arg3767,int arg3768,int arg3769,int arg3770,int arg3771,int arg3772,int arg3773,int arg3774,int arg3775,int arg3776,int arg3777,int arg3778,int arg3779,int arg3780,int arg3781,int arg3782,int arg3783,int arg3784,int arg3785,int arg3786,int arg3787,int arg3788,int arg3789,int arg3790,int arg3791,int arg3792,int arg3793,int arg3794,int arg3795,int arg3796,int arg3797,int arg3798,int arg3799,int arg3800,int arg3801,int arg3802,int arg3803,int arg3804,int arg3805,int arg3806,int arg3807,int arg3808,int arg3809,int arg3810,int arg3811,int arg3812,int arg3813,int arg3814,int arg3815,int arg3816,int arg3817,int arg3818,int arg3819,int arg3820,int arg3821,int arg3822,int arg3823,int arg3824,int arg3825,int arg3826,int arg3827,int arg3828,int arg3829,int arg3830,int arg3831,int arg3832,int arg3833,int arg3834,int arg3835,int arg3836,int arg3837,int arg3838,int arg3839,int arg3840,int arg3841,int arg3842,int arg3843,int arg3844,int arg3845,int arg3846,int arg3847,int arg3848,int arg3849,int arg3850,int arg3851,int arg3852,int arg3853,int arg3854,int arg3855,int arg3856,int arg3857,int arg3858,int arg3859,int arg3860,int arg3861,int arg3862,int arg3863,int arg3864,int arg3865,int arg3866,int arg3867,int arg3868,int arg3869,int arg3870,int arg3871,int arg3872,int arg3873,int arg3874,int arg3875,int arg3876,int arg3877,int arg3878,int arg3879,int arg3880,int arg3881,int arg3882,int arg3883,int arg3884,int arg3885,int arg3886,int arg3887,int arg3888,int arg3889,int arg3890,int arg3891,int arg3892,int arg3893,int arg3894,int arg3895,int arg3896,int arg3897,int arg3898,int arg3899,int arg3900,int arg3901,int arg3902,int arg3903,int arg3904,int arg3905,int arg3906,int arg3907,int arg3908,int arg3909,int arg3910,int arg3911,int arg3912,int arg3913,int arg3914,int arg3915,int arg3916,int arg3917,int arg3918,int arg3919,int arg3920,int arg3921,int arg3922,int arg3923,int arg3924,int arg3925,int arg3926,int arg3927,int arg3928,int arg3929,int arg3930,int arg3931,int arg3932,int arg3933,int arg3934,int arg3935,int arg3936,int arg3937,int arg3938,int arg3939,int arg3940,int arg3941,int arg3942,int arg3943,int arg3944,int arg3945,int arg3946,int arg3947,int arg3948,int arg3949,int arg3950,int arg3951,int arg3952,int arg3953,int arg3954,int arg3955,int arg3956,int arg3957,int arg3958,int arg3959,int arg3960,int arg3961,int arg3962,int arg3963,int arg3964,int arg3965,int arg3966,int arg3967,int arg3968,int arg3969,int arg3970,int arg3971,int arg3972,int arg3973,int arg3974,int arg3975,int arg3976,int arg3977,int arg3978,int arg3979,int arg3980,int arg3981,int arg3982,int arg3983,int arg3984,int arg3985,int arg3986,int arg3987,int arg3988,int arg3989,int arg3990,int arg3991,int arg3992,int arg3993,int arg3994,int arg3995,int arg3996,int arg3997,int arg3998,int arg3999,int arg4000,int arg4001,int arg4002,int arg4003,int arg4004,int arg4005,int arg4006,int arg4007,int arg4008,int arg4009,int arg4010,int arg4011,int arg4012,int arg4013,int arg4014,int arg4015,int arg4016,int arg4017,int arg4018,int arg4019,int arg4020,int arg4021,int arg4022,int arg4023,int arg4024,int arg4025,int arg4026,int arg4027,int arg4028,int arg4029,int arg4030,int arg4031,int arg4032,int arg4033,int arg4034,int arg4035,int arg4036,int arg4037,int arg4038,int arg4039,int arg4040,int arg4041,int arg4042,int arg4043,int arg4044,int arg4045,int arg4046,int arg4047,int arg4048,int arg4049,int arg4050,int arg4051,int arg4052,int arg4053,int arg4054,int arg4055,int arg4056,int arg4057,int arg4058,int arg4059,int arg4060,int arg4061,int arg4062,int arg4063,int arg4064,int arg4065,int arg4066,int arg4067,int arg4068,int arg4069,int arg4070,int arg4071,int arg4072,int arg4073,int arg4074,int arg4075,int arg4076,int arg4077,int arg4078,int arg4079,int arg4080,int arg4081,int arg4082,int arg4083,int arg4084,int arg4085,int arg4086,int arg4087,int arg4088,int arg4089,int arg4090,int arg4091,int arg4092,int arg4093,int arg4094,int arg4095,int arg4096,int arg4097,int arg4098,int arg4099,int arg4100,int arg4101,int arg4102,int arg4103,int arg4104,int arg4105,int arg4106,int arg4107,int arg4108,int arg4109,int arg4110,int arg4111,int arg4112,int arg4113,int arg4114,int arg4115,int arg4116,int arg4117,int arg4118,int arg4119,int arg4120,int arg4121,int arg4122,int arg4123,int arg4124,int arg4125,int arg4126,int arg4127,int arg4128,int arg4129,int arg4130,int arg4131,int arg4132,int arg4133,int arg4134,int arg4135,int arg4136,int arg4137,int arg4138,int arg4139,int arg4140,int arg4141,int arg4142,int arg4143,int arg4144,int arg4145,int arg4146,int arg4147,int arg4148,int arg4149,int arg4150,int arg4151,int arg4152,int arg4153,int arg4154,int arg4155,int arg4156,int arg4157,int arg4158,int arg4159,int arg4160,int arg4161,int arg4162,int arg4163,int arg4164,int arg4165,int arg4166,int arg4167,int arg4168,int arg4169,int arg4170,int arg4171,int arg4172,int arg4173,int arg4174,int arg4175,int arg4176,int arg4177,int arg4178,int arg4179,int arg4180,int arg4181,int arg4182,int arg4183,int arg4184,int arg4185,int arg4186,int arg4187,int arg4188,int arg4189,int arg4190,int arg4191,int arg4192,int arg4193,int arg4194,int arg4195,int arg4196,int arg4197,int arg4198,int arg4199,int arg4200,int arg4201,int arg4202,int arg4203,int arg4204,int arg4205,int arg4206,int arg4207,int arg4208,int arg4209,int arg4210,int arg4211,int arg4212,int arg4213,int arg4214,int arg4215,int arg4216,int arg4217,int arg4218,int arg4219,int arg4220,int arg4221,int arg4222,int arg4223,int arg4224,int arg4225,int arg4226,int arg4227,int arg4228,int arg4229,int arg4230,int arg4231,int arg4232,int arg4233,int arg4234,int arg4235,int arg4236,int arg4237,int arg4238,int arg4239,int arg4240,int arg4241,int arg4242,int arg4243,int arg4244,int arg4245,int arg4246,int arg4247,int arg4248,int arg4249,int arg4250,int arg4251,int arg4252,int arg4253,int arg4254,int arg4255,int arg4256,int arg4257,int arg4258,int arg4259,int arg4260,int arg4261,int arg4262,int arg4263,int arg4264,int arg4265,int arg4266,int arg4267,int arg4268,int arg4269,int arg4270,int arg4271,int arg4272,int arg4273,int arg4274,int arg4275,int arg4276,int arg4277,int arg4278,int arg4279,int arg4280,int arg4281,int arg4282,int arg4283,int arg4284,int arg4285,int arg4286,int arg4287,int arg4288,int arg4289,int arg4290,int arg4291,int arg4292,int arg4293,int arg4294,int arg4295,int arg4296,int arg4297,int arg4298,int arg4299,int arg4300,int arg4301,int arg4302,int arg4303,int arg4304,int arg4305,int arg4306,int arg4307,int arg4308,int arg4309,int arg4310,int arg4311,int arg4312,int arg4313,int arg4314,int arg4315,int arg4316,int arg4317,int arg4318,int arg4319,int arg4320,int arg4321,int arg4322,int arg4323,int arg4324,int arg4325,int arg4326,int arg4327,int arg4328,int arg4329,int arg4330,int arg4331,int arg4332,int arg4333,int arg4334,int arg4335,int arg4336,int arg4337,int arg4338,int arg4339,int arg4340,int arg4341,int arg4342,int arg4343,int arg4344,int arg4345,int arg4346,int arg4347,int arg4348,int arg4349,int arg4350,int arg4351,int arg4352,int arg4353,int arg4354,int arg4355,int arg4356,int arg4357,int arg4358,int arg4359,int arg4360,int arg4361,int arg4362,int arg4363,int arg4364,int arg4365,int arg4366,int arg4367,int arg4368,int arg4369,int arg4370,int arg4371,int arg4372,int arg4373,int arg4374,int arg4375,int arg4376,int arg4377,int arg4378,int arg4379,int arg4380,int arg4381,int arg4382,int arg4383,int arg4384,int arg4385,int arg4386,int arg4387,int arg4388,int arg4389,int arg4390,int arg4391,int arg4392,int arg4393,int arg4394,int arg4395,int arg4396,int arg4397,int arg4398,int arg4399,int arg4400,int arg4401,int arg4402,int arg4403,int arg4404,int arg4405,int arg4406,int arg4407,int arg4408,int arg4409,int arg4410,int arg4411,int arg4412,int arg4413,int arg4414,int arg4415,int arg4416,int arg4417,int arg4418,int arg4419,int arg4420,int arg4421,int arg4422,int arg4423,int arg4424,int arg4425,int arg4426,int arg4427,int arg4428,int arg4429,int arg4430,int arg4431,int arg4432,int arg4433,int arg4434,int arg4435,int arg4436,int arg4437,int arg4438,int arg4439,int arg4440,int arg4441,int arg4442,int arg4443,int arg4444,int arg4445,int arg4446,int arg4447,int arg4448,int arg4449,int arg4450,int arg4451,int arg4452,int arg4453,int arg4454,int arg4455,int arg4456,int arg4457,int arg4458,int arg4459,int arg4460,int arg4461,int arg4462,int arg4463,int arg4464,int arg4465,int arg4466,int arg4467,int arg4468,int arg4469,int arg4470,int arg4471,int arg4472,int arg4473,int arg4474,int arg4475,int arg4476,int arg4477,int arg4478,int arg4479,int arg4480,int arg4481,int arg4482,int arg4483,int arg4484,int arg4485,int arg4486,int arg4487,int arg4488,int arg4489,int arg4490,int arg4491,int arg4492,int arg4493,int arg4494,int arg4495,int arg4496,int arg4497,int arg4498,int arg4499,int arg4500,int arg4501,int arg4502,int arg4503,int arg4504,int arg4505,int arg4506,int arg4507,int arg4508,int arg4509,int arg4510,int arg4511,int arg4512,int arg4513,int arg4514,int arg4515,int arg4516,int arg4517,int arg4518,int arg4519,int arg4520,int arg4521,int arg4522,int arg4523,int arg4524,int arg4525,int arg4526,int arg4527,int arg4528,int arg4529,int arg4530,int arg4531,int arg4532,int arg4533,int arg4534,int arg4535,int arg4536,int arg4537,int arg4538,int arg4539,int arg4540,int arg4541,int arg4542,int arg4543,int arg4544,int arg4545,int arg4546,int arg4547,int arg4548,int arg4549,int arg4550,int arg4551,int arg4552,int arg4553,int arg4554,int arg4555,int arg4556,int arg4557,int arg4558,int arg4559,int arg4560,int arg4561,int arg4562,int arg4563,int arg4564,int arg4565,int arg4566,int arg4567,int arg4568,int arg4569,int arg4570,int arg4571,int arg4572,int arg4573,int arg4574,int arg4575,int arg4576,int arg4577,int arg4578,int arg4579,int arg4580,int arg4581,int arg4582,int arg4583,int arg4584,int arg4585,int arg4586,int arg4587,int arg4588,int arg4589,int arg4590,int arg4591,int arg4592,int arg4593,int arg4594,int arg4595,int arg4596,int arg4597,int arg4598,int arg4599,int arg4600,int arg4601,int arg4602,int arg4603,int arg4604,int arg4605,int arg4606,int arg4607,int arg4608,int arg4609,int arg4610,int arg4611,int arg4612,int arg4613,int arg4614,int arg4615,int arg4616,int arg4617,int arg4618,int arg4619,int arg4620,int arg4621,int arg4622,int arg4623,int arg4624,int arg4625,int arg4626,int arg4627,int arg4628,int arg4629,int arg4630,int arg4631,int arg4632,int arg4633,int arg4634,int arg4635,int arg4636,int arg4637,int arg4638,int arg4639,int arg4640,int arg4641,int arg4642,int arg4643,int arg4644,int arg4645,int arg4646,int arg4647,int arg4648,int arg4649,int arg4650,int arg4651,int arg4652,int arg4653,int arg4654,int arg4655,int arg4656,int arg4657,int arg4658,int arg4659,int arg4660,int arg4661,int arg4662,int arg4663,int arg4664,int arg4665,int arg4666,int arg4667,int arg4668,int arg4669,int arg4670,int arg4671,int arg4672,int arg4673,int arg4674,int arg4675,int arg4676,int arg4677,int arg4678,int arg4679,int arg4680,int arg4681,int arg4682,int arg4683,int arg4684,int arg4685,int arg4686,int arg4687,int arg4688,int arg4689,int arg4690,int arg4691,int arg4692,int arg4693,int arg4694,int arg4695,int arg4696,int arg4697,int arg4698,int arg4699,int arg4700,int arg4701,int arg4702,int arg4703,int arg4704,int arg4705,int arg4706,int arg4707,int arg4708,int arg4709,int arg4710,int arg4711,int arg4712,int arg4713,int arg4714,int arg4715,int arg4716,int arg4717,int arg4718,int arg4719,int arg4720,int arg4721,int arg4722,int arg4723,int arg4724,int arg4725,int arg4726,int arg4727,int arg4728,int arg4729,int arg4730,int arg4731,int arg4732,int arg4733,int arg4734,int arg4735,int arg4736,int arg4737,int arg4738,int arg4739,int arg4740,int arg4741,int arg4742,int arg4743,int arg4744,int arg4745,int arg4746,int arg4747,int arg4748,int arg4749,int arg4750,int arg4751,int arg4752,int arg4753,int arg4754,int arg4755,int arg4756,int arg4757,int arg4758,int arg4759,int arg4760,int arg4761,int arg4762,int arg4763,int arg4764,int arg4765,int arg4766,int arg4767,int arg4768,int arg4769,int arg4770,int arg4771,int arg4772,int arg4773,int arg4774,int arg4775,int arg4776,int arg4777,int arg4778,int arg4779,int arg4780,int arg4781,int arg4782,int arg4783,int arg4784,int arg4785,int arg4786,int arg4787,int arg4788,int arg4789,int arg4790,int arg4791,int arg4792,int arg4793,int arg4794,int arg4795,int arg4796,int arg4797,int arg4798,int arg4799,int arg4800,int arg4801,int arg4802,int arg4803,int arg4804,int arg4805,int arg4806,int arg4807,int arg4808,int arg4809,int arg4810,int arg4811,int arg4812,int arg4813,int arg4814,int arg4815,int arg4816,int arg4817,int arg4818,int arg4819,int arg4820,int arg4821,int arg4822,int arg4823,int arg4824,int arg4825,int arg4826,int arg4827,int arg4828,int arg4829,int arg4830,int arg4831,int arg4832,int arg4833,int arg4834,int arg4835,int arg4836,int arg4837,int arg4838,int arg4839,int arg4840,int arg4841,int arg4842,int arg4843,int arg4844,int arg4845,int arg4846,int arg4847,int arg4848,int arg4849,int arg4850,int arg4851,int arg4852,int arg4853,int arg4854,int arg4855,int arg4856,int arg4857,int arg4858,int arg4859,int arg4860,int arg4861,int arg4862,int arg4863,int arg4864,int arg4865,int arg4866,int arg4867,int arg4868,int arg4869,int arg4870,int arg4871,int arg4872,int arg4873,int arg4874,int arg4875,int arg4876,int arg4877,int arg4878,int arg4879,int arg4880,int arg4881,int arg4882,int arg4883,int arg4884,int arg4885,int arg4886,int arg4887,int arg4888,int arg4889,int arg4890,int arg4891,int arg4892,int arg4893,int arg4894,int arg4895,int arg4896,int arg4897,int arg4898,int arg4899,int arg4900,int arg4901,int arg4902,int arg4903,int arg4904,int arg4905,int arg4906,int arg4907,int arg4908,int arg4909,int arg4910,int arg4911,int arg4912,int arg4913,int arg4914,int arg4915,int arg4916,int arg4917,int arg4918,int arg4919,int arg4920,int arg4921,int arg4922,int arg4923,int arg4924,int arg4925,int arg4926,int arg4927,int arg4928,int arg4929,int arg4930,int arg4931,int arg4932,int arg4933,int arg4934,int arg4935,int arg4936,int arg4937,int arg4938,int arg4939,int arg4940,int arg4941,int arg4942,int arg4943,int arg4944,int arg4945,int arg4946,int arg4947,int arg4948,int arg4949,int arg4950,int arg4951,int arg4952,int arg4953,int arg4954,int arg4955,int arg4956,int arg4957,int arg4958,int arg4959,int arg4960,int arg4961,int arg4962,int arg4963,int arg4964,int arg4965,int arg4966,int arg4967,int arg4968,int arg4969,int arg4970,int arg4971,int arg4972,int arg4973,int arg4974,int arg4975,int arg4976,int arg4977,int arg4978,int arg4979,int arg4980,int arg4981,int arg4982,int arg4983,int arg4984,int arg4985,int arg4986,int arg4987,int arg4988,int arg4989,int arg4990,int arg4991,int arg4992,int arg4993,int arg4994,int arg4995,int arg4996,int arg4997,int arg4998,int arg4999) { return arg0+arg1+arg2+arg3+arg4+arg5+arg6+arg7+arg8+arg9+arg10+arg11+arg12+arg13+arg14+arg15+arg16+arg17+arg18+arg19+arg20+arg21+arg22+arg23+arg24+arg25+arg26+arg27+arg28+arg29+arg30+arg31+arg32+arg33+arg34+arg35+arg36+arg37+arg38+arg39+arg40+arg41+arg42+arg43+arg44+arg45+arg46+arg47+arg48+arg49+arg50+arg51+arg52+arg53+arg54+arg55+arg56+arg57+arg58+arg59+arg60+arg61+arg62+arg63+arg64+arg65+arg66+arg67+arg68+arg69+arg70+arg71+arg72+arg73+arg74+arg75+arg76+arg77+arg78+arg79+arg80+arg81+arg82+arg83+arg84+arg85+arg86+arg87+arg88+arg89+arg90+arg91+arg92+arg93+arg94+arg95+arg96+arg97+arg98+arg99+arg100+arg101+arg102+arg103+arg104+arg105+arg106+arg107+arg108+arg109+arg110+arg111+arg112+arg113+arg114+arg115+arg116+arg117+arg118+arg119+arg120+arg121+arg122+arg123+arg124+arg125+arg126+arg127+arg128+arg129+arg130+arg131+arg132+arg133+arg134+arg135+arg136+arg137+arg138+arg139+arg140+arg141+arg142+arg143+arg144+arg145+arg146+arg147+arg148+arg149+arg150+arg151+arg152+arg153+arg154+arg155+arg156+arg157+arg158+arg159+arg160+arg161+arg162+arg163+arg164+arg165+arg166+arg167+arg168+arg169+arg170+arg171+arg172+arg173+arg174+arg175+arg176+arg177+arg178+arg179+arg180+arg181+arg182+arg183+arg184+arg185+arg186+arg187+arg188+arg189+arg190+arg191+arg192+arg193+arg194+arg195+arg196+arg197+arg198+arg199+arg200+arg201+arg202+arg203+arg204+arg205+arg206+arg207+arg208+arg209+arg210+arg211+arg212+arg213+arg214+arg215+arg216+arg217+arg218+arg219+arg220+arg221+arg222+arg223+arg224+arg225+arg226+arg227+arg228+arg229+arg230+arg231+arg232+arg233+arg234+arg235+arg236+arg237+arg238+arg239+arg240+arg241+arg242+arg243+arg244+arg245+arg246+arg247+arg248+arg249+arg250+arg251+arg252+arg253+arg254+arg255+arg256+arg257+arg258+arg259+arg260+arg261+arg262+arg263+arg264+arg265+arg266+arg267+arg268+arg269+arg270+arg271+arg272+arg273+arg274+arg275+arg276+arg277+arg278+arg279+arg280+arg281+arg282+arg283+arg284+arg285+arg286+arg287+arg288+arg289+arg290+arg291+arg292+arg293+arg294+arg295+arg296+arg297+arg298+arg299+arg300+arg301+arg302+arg303+arg304+arg305+arg306+arg307+arg308+arg309+arg310+arg311+arg312+arg313+arg314+arg315+arg316+arg317+arg318+arg319+arg320+arg321+arg322+arg323+arg324+arg325+arg326+arg327+arg328+arg329+arg330+arg331+arg332+arg333+arg334+arg335+arg336+arg337+arg338+arg339+arg340+arg341+arg342+arg343+arg344+arg345+arg346+arg347+arg348+arg349+arg350+arg351+arg352+arg353+arg354+arg355+arg356+arg357+arg358+arg359+arg360+arg361+arg362+arg363+arg364+arg365+arg366+arg367+arg368+arg369+arg370+arg371+arg372+arg373+arg374+arg375+arg376+arg377+arg378+arg379+arg380+arg381+arg382+arg383+arg384+arg385+arg386+arg387+arg388+arg389+arg390+arg391+arg392+arg393+arg394+arg395+arg396+arg397+arg398+arg399+arg400+arg401+arg402+arg403+arg404+arg405+arg406+arg407+arg408+arg409+arg410+arg411+arg412+arg413+arg414+arg415+arg416+arg417+arg418+arg419+arg420+arg421+arg422+arg423+arg424+arg425+arg426+arg427+arg428+arg429+arg430+arg431+arg432+arg433+arg434+arg435+arg436+arg437+arg438+arg439+arg440+arg441+arg442+arg443+arg444+arg445+arg446+arg447+arg448+arg449+arg450+arg451+arg452+arg453+arg454+arg455+arg456+arg457+arg458+arg459+arg460+arg461+arg462+arg463+arg464+arg465+arg466+arg467+arg468+arg469+arg470+arg471+arg472+arg473+arg474+arg475+arg476+arg477+arg478+arg479+arg480+arg481+arg482+arg483+arg484+arg485+arg486+arg487+arg488+arg489+arg490+arg491+arg492+arg493+arg494+arg495+arg496+arg497+arg498+arg499+arg500+arg501+arg502+arg503+arg504+arg505+arg506+arg507+arg508+arg509+arg510+arg511+arg512+arg513+arg514+arg515+arg516+arg517+arg518+arg519+arg520+arg521+arg522+arg523+arg524+arg525+arg526+arg527+arg528+arg529+arg530+arg531+arg532+arg533+arg534+arg535+arg536+arg537+arg538+arg539+arg540+arg541+arg542+arg543+arg544+arg545+arg546+arg547+arg548+arg549+arg550+arg551+arg552+arg553+arg554+arg555+arg556+arg557+arg558+arg559+arg560+arg561+arg562+arg563+arg564+arg565+arg566+arg567+arg568+arg569+arg570+arg571+arg572+arg573+arg574+arg575+arg576+arg577+arg578+arg579+arg580+arg581+arg582+arg583+arg584+arg585+arg586+arg587+arg588+arg589+arg590+arg591+arg592+arg593+arg594+arg595+arg596+arg597+arg598+arg599+arg600+arg601+arg602+arg603+arg604+arg605+arg606+arg607+arg608+arg609+arg610+arg611+arg612+arg613+arg614+arg615+arg616+arg617+arg618+arg619+arg620+arg621+arg622+arg623+arg624+arg625+arg626+arg627+arg628+arg629+arg630+arg631+arg632+arg633+arg634+arg635+arg636+arg637+arg638+arg639+arg640+arg641+arg642+arg643+arg644+arg645+arg646+arg647+arg648+arg649+arg650+arg651+arg652+arg653+arg654+arg655+arg656+arg657+arg658+arg659+arg660+arg661+arg662+arg663+arg664+arg665+arg666+arg667+arg668+arg669+arg670+arg671+arg672+arg673+arg674+arg675+arg676+arg677+arg678+arg679+arg680+arg681+arg682+arg683+arg684+arg685+arg686+arg687+arg688+arg689+arg690+arg691+arg692+arg693+arg694+arg695+arg696+arg697+arg698+arg699+arg700+arg701+arg702+arg703+arg704+arg705+arg706+arg707+arg708+arg709+arg710+arg711+arg712+arg713+arg714+arg715+arg716+arg717+arg718+arg719+arg720+arg721+arg722+arg723+arg724+arg725+arg726+arg727+arg728+arg729+arg730+arg731+arg732+arg733+arg734+arg735+arg736+arg737+arg738+arg739+arg740+arg741+arg742+arg743+arg744+arg745+arg746+arg747+arg748+arg749+arg750+arg751+arg752+arg753+arg754+arg755+arg756+arg757+arg758+arg759+arg760+arg761+arg762+arg763+arg764+arg765+arg766+arg767+arg768+arg769+arg770+arg771+arg772+arg773+arg774+arg775+arg776+arg777+arg778+arg779+arg780+arg781+arg782+arg783+arg784+arg785+arg786+arg787+arg788+arg789+arg790+arg791+arg792+arg793+arg794+arg795+arg796+arg797+arg798+arg799+arg800+arg801+arg802+arg803+arg804+arg805+arg806+arg807+arg808+arg809+arg810+arg811+arg812+arg813+arg814+arg815+arg816+arg817+arg818+arg819+arg820+arg821+arg822+arg823+arg824+arg825+arg826+arg827+arg828+arg829+arg830+arg831+arg832+arg833+arg834+arg835+arg836+arg837+arg838+arg839+arg840+arg841+arg842+arg843+arg844+arg845+arg846+arg847+arg848+arg849+arg850+arg851+arg852+arg853+arg854+arg855+arg856+arg857+arg858+arg859+arg860+arg861+arg862+arg863+arg864+arg865+arg866+arg867+arg868+arg869+arg870+arg871+arg872+arg873+arg874+arg875+arg876+arg877+arg878+arg879+arg880+arg881+arg882+arg883+arg884+arg885+arg886+arg887+arg888+arg889+arg890+arg891+arg892+arg893+arg894+arg895+arg896+arg897+arg898+arg899+arg900+arg901+arg902+arg903+arg904+arg905+arg906+arg907+arg908+arg909+arg910+arg911+arg912+arg913+arg914+arg915+arg916+arg917+arg918+arg919+arg920+arg921+arg922+arg923+arg924+arg925+arg926+arg927+arg928+arg929+arg930+arg931+arg932+arg933+arg934+arg935+arg936+arg937+arg938+arg939+arg940+arg941+arg942+arg943+arg944+arg945+arg946+arg947+arg948+arg949+arg950+arg951+arg952+arg953+arg954+arg955+arg956+arg957+arg958+arg959+arg960+arg961+arg962+arg963+arg964+arg965+arg966+arg967+arg968+arg969+arg970+arg971+arg972+arg973+arg974+arg975+arg976+arg977+arg978+arg979+arg980+arg981+arg982+arg983+arg984+arg985+arg986+arg987+arg988+arg989+arg990+arg991+arg992+arg993+arg994+arg995+arg996+arg997+arg998+arg999+arg1000+arg1001+arg1002+arg1003+arg1004+arg1005+arg1006+arg1007+arg1008+arg1009+arg1010+arg1011+arg1012+arg1013+arg1014+arg1015+arg1016+arg1017+arg1018+arg1019+arg1020+arg1021+arg1022+arg1023+arg1024+arg1025+arg1026+arg1027+arg1028+arg1029+arg1030+arg1031+arg1032+arg1033+arg1034+arg1035+arg1036+arg1037+arg1038+arg1039+arg1040+arg1041+arg1042+arg1043+arg1044+arg1045+arg1046+arg1047+arg1048+arg1049+arg1050+arg1051+arg1052+arg1053+arg1054+arg1055+arg1056+arg1057+arg1058+arg1059+arg1060+arg1061+arg1062+arg1063+arg1064+arg1065+arg1066+arg1067+arg1068+arg1069+arg1070+arg1071+arg1072+arg1073+arg1074+arg1075+arg1076+arg1077+arg1078+arg1079+arg1080+arg1081+arg1082+arg1083+arg1084+arg1085+arg1086+arg1087+arg1088+arg1089+arg1090+arg1091+arg1092+arg1093+arg1094+arg1095+arg1096+arg1097+arg1098+arg1099+arg1100+arg1101+arg1102+arg1103+arg1104+arg1105+arg1106+arg1107+arg1108+arg1109+arg1110+arg1111+arg1112+arg1113+arg1114+arg1115+arg1116+arg1117+arg1118+arg1119+arg1120+arg1121+arg1122+arg1123+arg1124+arg1125+arg1126+arg1127+arg1128+arg1129+arg1130+arg1131+arg1132+arg1133+arg1134+arg1135+arg1136+arg1137+arg1138+arg1139+arg1140+arg1141+arg1142+arg1143+arg1144+arg1145+arg1146+arg1147+arg1148+arg1149+arg1150+arg1151+arg1152+arg1153+arg1154+arg1155+arg1156+arg1157+arg1158+arg1159+arg1160+arg1161+arg1162+arg1163+arg1164+arg1165+arg1166+arg1167+arg1168+arg1169+arg1170+arg1171+arg1172+arg1173+arg1174+arg1175+arg1176+arg1177+arg1178+arg1179+arg1180+arg1181+arg1182+arg1183+arg1184+arg1185+arg1186+arg1187+arg1188+arg1189+arg1190+arg1191+arg1192+arg1193+arg1194+arg1195+arg1196+arg1197+arg1198+arg1199+arg1200+arg1201+arg1202+arg1203+arg1204+arg1205+arg1206+arg1207+arg1208+arg1209+arg1210+arg1211+arg1212+arg1213+arg1214+arg1215+arg1216+arg1217+arg1218+arg1219+arg1220+arg1221+arg1222+arg1223+arg1224+arg1225+arg1226+arg1227+arg1228+arg1229+arg1230+arg1231+arg1232+arg1233+arg1234+arg1235+arg1236+arg1237+arg1238+arg1239+arg1240+arg1241+arg1242+arg1243+arg1244+arg1245+arg1246+arg1247+arg1248+arg1249+arg1250+arg1251+arg1252+arg1253+arg1254+arg1255+arg1256+arg1257+arg1258+arg1259+arg1260+arg1261+arg1262+arg1263+arg1264+arg1265+arg1266+arg1267+arg1268+arg1269+arg1270+arg1271+arg1272+arg1273+arg1274+arg1275+arg1276+arg1277+arg1278+arg1279+arg1280+arg1281+arg1282+arg1283+arg1284+arg1285+arg1286+arg1287+arg1288+arg1289+arg1290+arg1291+arg1292+arg1293+arg1294+arg1295+arg1296+arg1297+arg1298+arg1299+arg1300+arg1301+arg1302+arg1303+arg1304+arg1305+arg1306+arg1307+arg1308+arg1309+arg1310+arg1311+arg1312+arg1313+arg1314+arg1315+arg1316+arg1317+arg1318+arg1319+arg1320+arg1321+arg1322+arg1323+arg1324+arg1325+arg1326+arg1327+arg1328+arg1329+arg1330+arg1331+arg1332+arg1333+arg1334+arg1335+arg1336+arg1337+arg1338+arg1339+arg1340+arg1341+arg1342+arg1343+arg1344+arg1345+arg1346+arg1347+arg1348+arg1349+arg1350+arg1351+arg1352+arg1353+arg1354+arg1355+arg1356+arg1357+arg1358+arg1359+arg1360+arg1361+arg1362+arg1363+arg1364+arg1365+arg1366+arg1367+arg1368+arg1369+arg1370+arg1371+arg1372+arg1373+arg1374+arg1375+arg1376+arg1377+arg1378+arg1379+arg1380+arg1381+arg1382+arg1383+arg1384+arg1385+arg1386+arg1387+arg1388+arg1389+arg1390+arg1391+arg1392+arg1393+arg1394+arg1395+arg1396+arg1397+arg1398+arg1399+arg1400+arg1401+arg1402+arg1403+arg1404+arg1405+arg1406+arg1407+arg1408+arg1409+arg1410+arg1411+arg1412+arg1413+arg1414+arg1415+arg1416+arg1417+arg1418+arg1419+arg1420+arg1421+arg1422+arg1423+arg1424+arg1425+arg1426+arg1427+arg1428+arg1429+arg1430+arg1431+arg1432+arg1433+arg1434+arg1435+arg1436+arg1437+arg1438+arg1439+arg1440+arg1441+arg1442+arg1443+arg1444+arg1445+arg1446+arg1447+arg1448+arg1449+arg1450+arg1451+arg1452+arg1453+arg1454+arg1455+arg1456+arg1457+arg1458+arg1459+arg1460+arg1461+arg1462+arg1463+arg1464+arg1465+arg1466+arg1467+arg1468+arg1469+arg1470+arg1471+arg1472+arg1473+arg1474+arg1475+arg1476+arg1477+arg1478+arg1479+arg1480+arg1481+arg1482+arg1483+arg1484+arg1485+arg1486+arg1487+arg1488+arg1489+arg1490+arg1491+arg1492+arg1493+arg1494+arg1495+arg1496+arg1497+arg1498+arg1499+arg1500+arg1501+arg1502+arg1503+arg1504+arg1505+arg1506+arg1507+arg1508+arg1509+arg1510+arg1511+arg1512+arg1513+arg1514+arg1515+arg1516+arg1517+arg1518+arg1519+arg1520+arg1521+arg1522+arg1523+arg1524+arg1525+arg1526+arg1527+arg1528+arg1529+arg1530+arg1531+arg1532+arg1533+arg1534+arg1535+arg1536+arg1537+arg1538+arg1539+arg1540+arg1541+arg1542+arg1543+arg1544+arg1545+arg1546+arg1547+arg1548+arg1549+arg1550+arg1551+arg1552+arg1553+arg1554+arg1555+arg1556+arg1557+arg1558+arg1559+arg1560+arg1561+arg1562+arg1563+arg1564+arg1565+arg1566+arg1567+arg1568+arg1569+arg1570+arg1571+arg1572+arg1573+arg1574+arg1575+arg1576+arg1577+arg1578+arg1579+arg1580+arg1581+arg1582+arg1583+arg1584+arg1585+arg1586+arg1587+arg1588+arg1589+arg1590+arg1591+arg1592+arg1593+arg1594+arg1595+arg1596+arg1597+arg1598+arg1599+arg1600+arg1601+arg1602+arg1603+arg1604+arg1605+arg1606+arg1607+arg1608+arg1609+arg1610+arg1611+arg1612+arg1613+arg1614+arg1615+arg1616+arg1617+arg1618+arg1619+arg1620+arg1621+arg1622+arg1623+arg1624+arg1625+arg1626+arg1627+arg1628+arg1629+arg1630+arg1631+arg1632+arg1633+arg1634+arg1635+arg1636+arg1637+arg1638+arg1639+arg1640+arg1641+arg1642+arg1643+arg1644+arg1645+arg1646+arg1647+arg1648+arg1649+arg1650+arg1651+arg1652+arg1653+arg1654+arg1655+arg1656+arg1657+arg1658+arg1659+arg1660+arg1661+arg1662+arg1663+arg1664+arg1665+arg1666+arg1667+arg1668+arg1669+arg1670+arg1671+arg1672+arg1673+arg1674+arg1675+arg1676+arg1677+arg1678+arg1679+arg1680+arg1681+arg1682+arg1683+arg1684+arg1685+arg1686+arg1687+arg1688+arg1689+arg1690+arg1691+arg1692+arg1693+arg1694+arg1695+arg1696+arg1697+arg1698+arg1699+arg1700+arg1701+arg1702+arg1703+arg1704+arg1705+arg1706+arg1707+arg1708+arg1709+arg1710+arg1711+arg1712+arg1713+arg1714+arg1715+arg1716+arg1717+arg1718+arg1719+arg1720+arg1721+arg1722+arg1723+arg1724+arg1725+arg1726+arg1727+arg1728+arg1729+arg1730+arg1731+arg1732+arg1733+arg1734+arg1735+arg1736+arg1737+arg1738+arg1739+arg1740+arg1741+arg1742+arg1743+arg1744+arg1745+arg1746+arg1747+arg1748+arg1749+arg1750+arg1751+arg1752+arg1753+arg1754+arg1755+arg1756+arg1757+arg1758+arg1759+arg1760+arg1761+arg1762+arg1763+arg1764+arg1765+arg1766+arg1767+arg1768+arg1769+arg1770+arg1771+arg1772+arg1773+arg1774+arg1775+arg1776+arg1777+arg1778+arg1779+arg1780+arg1781+arg1782+arg1783+arg1784+arg1785+arg1786+arg1787+arg1788+arg1789+arg1790+arg1791+arg1792+arg1793+arg1794+arg1795+arg1796+arg1797+arg1798+arg1799+arg1800+arg1801+arg1802+arg1803+arg1804+arg1805+arg1806+arg1807+arg1808+arg1809+arg1810+arg1811+arg1812+arg1813+arg1814+arg1815+arg1816+arg1817+arg1818+arg1819+arg1820+arg1821+arg1822+arg1823+arg1824+arg1825+arg1826+arg1827+arg1828+arg1829+arg1830+arg1831+arg1832+arg1833+arg1834+arg1835+arg1836+arg1837+arg1838+arg1839+arg1840+arg1841+arg1842+arg1843+arg1844+arg1845+arg1846+arg1847+arg1848+arg1849+arg1850+arg1851+arg1852+arg1853+arg1854+arg1855+arg1856+arg1857+arg1858+arg1859+arg1860+arg1861+arg1862+arg1863+arg1864+arg1865+arg1866+arg1867+arg1868+arg1869+arg1870+arg1871+arg1872+arg1873+arg1874+arg1875+arg1876+arg1877+arg1878+arg1879+arg1880+arg1881+arg1882+arg1883+arg1884+arg1885+arg1886+arg1887+arg1888+arg1889+arg1890+arg1891+arg1892+arg1893+arg1894+arg1895+arg1896+arg1897+arg1898+arg1899+arg1900+arg1901+arg1902+arg1903+arg1904+arg1905+arg1906+arg1907+arg1908+arg1909+arg1910+arg1911+arg1912+arg1913+arg1914+arg1915+arg1916+arg1917+arg1918+arg1919+arg1920+arg1921+arg1922+arg1923+arg1924+arg1925+arg1926+arg1927+arg1928+arg1929+arg1930+arg1931+arg1932+arg1933+arg1934+arg1935+arg1936+arg1937+arg1938+arg1939+arg1940+arg1941+arg1942+arg1943+arg1944+arg1945+arg1946+arg1947+arg1948+arg1949+arg1950+arg1951+arg1952+arg1953+arg1954+arg1955+arg1956+arg1957+arg1958+arg1959+arg1960+arg1961+arg1962+arg1963+arg1964+arg1965+arg1966+arg1967+arg1968+arg1969+arg1970+arg1971+arg1972+arg1973+arg1974+arg1975+arg1976+arg1977+arg1978+arg1979+arg1980+arg1981+arg1982+arg1983+arg1984+arg1985+arg1986+arg1987+arg1988+arg1989+arg1990+arg1991+arg1992+arg1993+arg1994+arg1995+arg1996+arg1997+arg1998+arg1999+arg2000+arg2001+arg2002+arg2003+arg2004+arg2005+arg2006+arg2007+arg2008+arg2009+arg2010+arg2011+arg2012+arg2013+arg2014+arg2015+arg2016+arg2017+arg2018+arg2019+arg2020+arg2021+arg2022+arg2023+arg2024+arg2025+arg2026+arg2027+arg2028+arg2029+arg2030+arg2031+arg2032+arg2033+arg2034+arg2035+arg2036+arg2037+arg2038+arg2039+arg2040+arg2041+arg2042+arg2043+arg2044+arg2045+arg2046+arg2047+arg2048+arg2049+arg2050+arg2051+arg2052+arg2053+arg2054+arg2055+arg2056+arg2057+arg2058+arg2059+arg2060+arg2061+arg2062+arg2063+arg2064+arg2065+arg2066+arg2067+arg2068+arg2069+arg2070+arg2071+arg2072+arg2073+arg2074+arg2075+arg2076+arg2077+arg2078+arg2079+arg2080+arg2081+arg2082+arg2083+arg2084+arg2085+arg2086+arg2087+arg2088+arg2089+arg2090+arg2091+arg2092+arg2093+arg2094+arg2095+arg2096+arg2097+arg2098+arg2099+arg2100+arg2101+arg2102+arg2103+arg2104+arg2105+arg2106+arg2107+arg2108+arg2109+arg2110+arg2111+arg2112+arg2113+arg2114+arg2115+arg2116+arg2117+arg2118+arg2119+arg2120+arg2121+arg2122+arg2123+arg2124+arg2125+arg2126+arg2127+arg2128+arg2129+arg2130+arg2131+arg2132+arg2133+arg2134+arg2135+arg2136+arg2137+arg2138+arg2139+arg2140+arg2141+arg2142+arg2143+arg2144+arg2145+arg2146+arg2147+arg2148+arg2149+arg2150+arg2151+arg2152+arg2153+arg2154+arg2155+arg2156+arg2157+arg2158+arg2159+arg2160+arg2161+arg2162+arg2163+arg2164+arg2165+arg2166+arg2167+arg2168+arg2169+arg2170+arg2171+arg2172+arg2173+arg2174+arg2175+arg2176+arg2177+arg2178+arg2179+arg2180+arg2181+arg2182+arg2183+arg2184+arg2185+arg2186+arg2187+arg2188+arg2189+arg2190+arg2191+arg2192+arg2193+arg2194+arg2195+arg2196+arg2197+arg2198+arg2199+arg2200+arg2201+arg2202+arg2203+arg2204+arg2205+arg2206+arg2207+arg2208+arg2209+arg2210+arg2211+arg2212+arg2213+arg2214+arg2215+arg2216+arg2217+arg2218+arg2219+arg2220+arg2221+arg2222+arg2223+arg2224+arg2225+arg2226+arg2227+arg2228+arg2229+arg2230+arg2231+arg2232+arg2233+arg2234+arg2235+arg2236+arg2237+arg2238+arg2239+arg2240+arg2241+arg2242+arg2243+arg2244+arg2245+arg2246+arg2247+arg2248+arg2249+arg2250+arg2251+arg2252+arg2253+arg2254+arg2255+arg2256+arg2257+arg2258+arg2259+arg2260+arg2261+arg2262+arg2263+arg2264+arg2265+arg2266+arg2267+arg2268+arg2269+arg2270+arg2271+arg2272+arg2273+arg2274+arg2275+arg2276+arg2277+arg2278+arg2279+arg2280+arg2281+arg2282+arg2283+arg2284+arg2285+arg2286+arg2287+arg2288+arg2289+arg2290+arg2291+arg2292+arg2293+arg2294+arg2295+arg2296+arg2297+arg2298+arg2299+arg2300+arg2301+arg2302+arg2303+arg2304+arg2305+arg2306+arg2307+arg2308+arg2309+arg2310+arg2311+arg2312+arg2313+arg2314+arg2315+arg2316+arg2317+arg2318+arg2319+arg2320+arg2321+arg2322+arg2323+arg2324+arg2325+arg2326+arg2327+arg2328+arg2329+arg2330+arg2331+arg2332+arg2333+arg2334+arg2335+arg2336+arg2337+arg2338+arg2339+arg2340+arg2341+arg2342+arg2343+arg2344+arg2345+arg2346+arg2347+arg2348+arg2349+arg2350+arg2351+arg2352+arg2353+arg2354+arg2355+arg2356+arg2357+arg2358+arg2359+arg2360+arg2361+arg2362+arg2363+arg2364+arg2365+arg2366+arg2367+arg2368+arg2369+arg2370+arg2371+arg2372+arg2373+arg2374+arg2375+arg2376+arg2377+arg2378+arg2379+arg2380+arg2381+arg2382+arg2383+arg2384+arg2385+arg2386+arg2387+arg2388+arg2389+arg2390+arg2391+arg2392+arg2393+arg2394+arg2395+arg2396+arg2397+arg2398+arg2399+arg2400+arg2401+arg2402+arg2403+arg2404+arg2405+arg2406+arg2407+arg2408+arg2409+arg2410+arg2411+arg2412+arg2413+arg2414+arg2415+arg2416+arg2417+arg2418+arg2419+arg2420+arg2421+arg2422+arg2423+arg2424+arg2425+arg2426+arg2427+arg2428+arg2429+arg2430+arg2431+arg2432+arg2433+arg2434+arg2435+arg2436+arg2437+arg2438+arg2439+arg2440+arg2441+arg2442+arg2443+arg2444+arg2445+arg2446+arg2447+arg2448+arg2449+arg2450+arg2451+arg2452+arg2453+arg2454+arg2455+arg2456+arg2457+arg2458+arg2459+arg2460+arg2461+arg2462+arg2463+arg2464+arg2465+arg2466+arg2467+arg2468+arg2469+arg2470+arg2471+arg2472+arg2473+arg2474+arg2475+arg2476+arg2477+arg2478+arg2479+arg2480+arg2481+arg2482+arg2483+arg2484+arg2485+arg2486+arg2487+arg2488+arg2489+arg2490+arg2491+arg2492+arg2493+arg2494+arg2495+arg2496+arg2497+arg2498+arg2499+arg2500+arg2501+arg2502+arg2503+arg2504+arg2505+arg2506+arg2507+arg2508+arg2509+arg2510+arg2511+arg2512+arg2513+arg2514+arg2515+arg2516+arg2517+arg2518+arg2519+arg2520+arg2521+arg2522+arg2523+arg2524+arg2525+arg2526+arg2527+arg2528+arg2529+arg2530+arg2531+arg2532+arg2533+arg2534+arg2535+arg2536+arg2537+arg2538+arg2539+arg2540+arg2541+arg2542+arg2543+arg2544+arg2545+arg2546+arg2547+arg2548+arg2549+arg2550+arg2551+arg2552+arg2553+arg2554+arg2555+arg2556+arg2557+arg2558+arg2559+arg2560+arg2561+arg2562+arg2563+arg2564+arg2565+arg2566+arg2567+arg2568+arg2569+arg2570+arg2571+arg2572+arg2573+arg2574+arg2575+arg2576+arg2577+arg2578+arg2579+arg2580+arg2581+arg2582+arg2583+arg2584+arg2585+arg2586+arg2587+arg2588+arg2589+arg2590+arg2591+arg2592+arg2593+arg2594+arg2595+arg2596+arg2597+arg2598+arg2599+arg2600+arg2601+arg2602+arg2603+arg2604+arg2605+arg2606+arg2607+arg2608+arg2609+arg2610+arg2611+arg2612+arg2613+arg2614+arg2615+arg2616+arg2617+arg2618+arg2619+arg2620+arg2621+arg2622+arg2623+arg2624+arg2625+arg2626+arg2627+arg2628+arg2629+arg2630+arg2631+arg2632+arg2633+arg2634+arg2635+arg2636+arg2637+arg2638+arg2639+arg2640+arg2641+arg2642+arg2643+arg2644+arg2645+arg2646+arg2647+arg2648+arg2649+arg2650+arg2651+arg2652+arg2653+arg2654+arg2655+arg2656+arg2657+arg2658+arg2659+arg2660+arg2661+arg2662+arg2663+arg2664+arg2665+arg2666+arg2667+arg2668+arg2669+arg2670+arg2671+arg2672+arg2673+arg2674+arg2675+arg2676+arg2677+arg2678+arg2679+arg2680+arg2681+arg2682+arg2683+arg2684+arg2685+arg2686+arg2687+arg2688+arg2689+arg2690+arg2691+arg2692+arg2693+arg2694+arg2695+arg2696+arg2697+arg2698+arg2699+arg2700+arg2701+arg2702+arg2703+arg2704+arg2705+arg2706+arg2707+arg2708+arg2709+arg2710+arg2711+arg2712+arg2713+arg2714+arg2715+arg2716+arg2717+arg2718+arg2719+arg2720+arg2721+arg2722+arg2723+arg2724+arg2725+arg2726+arg2727+arg2728+arg2729+arg2730+arg2731+arg2732+arg2733+arg2734+arg2735+arg2736+arg2737+arg2738+arg2739+arg2740+arg2741+arg2742+arg2743+arg2744+arg2745+arg2746+arg2747+arg2748+arg2749+arg2750+arg2751+arg2752+arg2753+arg2754+arg2755+arg2756+arg2757+arg2758+arg2759+arg2760+arg2761+arg2762+arg2763+arg2764+arg2765+arg2766+arg2767+arg2768+arg2769+arg2770+arg2771+arg2772+arg2773+arg2774+arg2775+arg2776+arg2777+arg2778+arg2779+arg2780+arg2781+arg2782+arg2783+arg2784+arg2785+arg2786+arg2787+arg2788+arg2789+arg2790+arg2791+arg2792+arg2793+arg2794+arg2795+arg2796+arg2797+arg2798+arg2799+arg2800+arg2801+arg2802+arg2803+arg2804+arg2805+arg2806+arg2807+arg2808+arg2809+arg2810+arg2811+arg2812+arg2813+arg2814+arg2815+arg2816+arg2817+arg2818+arg2819+arg2820+arg2821+arg2822+arg2823+arg2824+arg2825+arg2826+arg2827+arg2828+arg2829+arg2830+arg2831+arg2832+arg2833+arg2834+arg2835+arg2836+arg2837+arg2838+arg2839+arg2840+arg2841+arg2842+arg2843+arg2844+arg2845+arg2846+arg2847+arg2848+arg2849+arg2850+arg2851+arg2852+arg2853+arg2854+arg2855+arg2856+arg2857+arg2858+arg2859+arg2860+arg2861+arg2862+arg2863+arg2864+arg2865+arg2866+arg2867+arg2868+arg2869+arg2870+arg2871+arg2872+arg2873+arg2874+arg2875+arg2876+arg2877+arg2878+arg2879+arg2880+arg2881+arg2882+arg2883+arg2884+arg2885+arg2886+arg2887+arg2888+arg2889+arg2890+arg2891+arg2892+arg2893+arg2894+arg2895+arg2896+arg2897+arg2898+arg2899+arg2900+arg2901+arg2902+arg2903+arg2904+arg2905+arg2906+arg2907+arg2908+arg2909+arg2910+arg2911+arg2912+arg2913+arg2914+arg2915+arg2916+arg2917+arg2918+arg2919+arg2920+arg2921+arg2922+arg2923+arg2924+arg2925+arg2926+arg2927+arg2928+arg2929+arg2930+arg2931+arg2932+arg2933+arg2934+arg2935+arg2936+arg2937+arg2938+arg2939+arg2940+arg2941+arg2942+arg2943+arg2944+arg2945+arg2946+arg2947+arg2948+arg2949+arg2950+arg2951+arg2952+arg2953+arg2954+arg2955+arg2956+arg2957+arg2958+arg2959+arg2960+arg2961+arg2962+arg2963+arg2964+arg2965+arg2966+arg2967+arg2968+arg2969+arg2970+arg2971+arg2972+arg2973+arg2974+arg2975+arg2976+arg2977+arg2978+arg2979+arg2980+arg2981+arg2982+arg2983+arg2984+arg2985+arg2986+arg2987+arg2988+arg2989+arg2990+arg2991+arg2992+arg2993+arg2994+arg2995+arg2996+arg2997+arg2998+arg2999+arg3000+arg3001+arg3002+arg3003+arg3004+arg3005+arg3006+arg3007+arg3008+arg3009+arg3010+arg3011+arg3012+arg3013+arg3014+arg3015+arg3016+arg3017+arg3018+arg3019+arg3020+arg3021+arg3022+arg3023+arg3024+arg3025+arg3026+arg3027+arg3028+arg3029+arg3030+arg3031+arg3032+arg3033+arg3034+arg3035+arg3036+arg3037+arg3038+arg3039+arg3040+arg3041+arg3042+arg3043+arg3044+arg3045+arg3046+arg3047+arg3048+arg3049+arg3050+arg3051+arg3052+arg3053+arg3054+arg3055+arg3056+arg3057+arg3058+arg3059+arg3060+arg3061+arg3062+arg3063+arg3064+arg3065+arg3066+arg3067+arg3068+arg3069+arg3070+arg3071+arg3072+arg3073+arg3074+arg3075+arg3076+arg3077+arg3078+arg3079+arg3080+arg3081+arg3082+arg3083+arg3084+arg3085+arg3086+arg3087+arg3088+arg3089+arg3090+arg3091+arg3092+arg3093+arg3094+arg3095+arg3096+arg3097+arg3098+arg3099+arg3100+arg3101+arg3102+arg3103+arg3104+arg3105+arg3106+arg3107+arg3108+arg3109+arg3110+arg3111+arg3112+arg3113+arg3114+arg3115+arg3116+arg3117+arg3118+arg3119+arg3120+arg3121+arg3122+arg3123+arg3124+arg3125+arg3126+arg3127+arg3128+arg3129+arg3130+arg3131+arg3132+arg3133+arg3134+arg3135+arg3136+arg3137+arg3138+arg3139+arg3140+arg3141+arg3142+arg3143+arg3144+arg3145+arg3146+arg3147+arg3148+arg3149+arg3150+arg3151+arg3152+arg3153+arg3154+arg3155+arg3156+arg3157+arg3158+arg3159+arg3160+arg3161+arg3162+arg3163+arg3164+arg3165+arg3166+arg3167+arg3168+arg3169+arg3170+arg3171+arg3172+arg3173+arg3174+arg3175+arg3176+arg3177+arg3178+arg3179+arg3180+arg3181+arg3182+arg3183+arg3184+arg3185+arg3186+arg3187+arg3188+arg3189+arg3190+arg3191+arg3192+arg3193+arg3194+arg3195+arg3196+arg3197+arg3198+arg3199+arg3200+arg3201+arg3202+arg3203+arg3204+arg3205+arg3206+arg3207+arg3208+arg3209+arg3210+arg3211+arg3212+arg3213+arg3214+arg3215+arg3216+arg3217+arg3218+arg3219+arg3220+arg3221+arg3222+arg3223+arg3224+arg3225+arg3226+arg3227+arg3228+arg3229+arg3230+arg3231+arg3232+arg3233+arg3234+arg3235+arg3236+arg3237+arg3238+arg3239+arg3240+arg3241+arg3242+arg3243+arg3244+arg3245+arg3246+arg3247+arg3248+arg3249+arg3250+arg3251+arg3252+arg3253+arg3254+arg3255+arg3256+arg3257+arg3258+arg3259+arg3260+arg3261+arg3262+arg3263+arg3264+arg3265+arg3266+arg3267+arg3268+arg3269+arg3270+arg3271+arg3272+arg3273+arg3274+arg3275+arg3276+arg3277+arg3278+arg3279+arg3280+arg3281+arg3282+arg3283+arg3284+arg3285+arg3286+arg3287+arg3288+arg3289+arg3290+arg3291+arg3292+arg3293+arg3294+arg3295+arg3296+arg3297+arg3298+arg3299+arg3300+arg3301+arg3302+arg3303+arg3304+arg3305+arg3306+arg3307+arg3308+arg3309+arg3310+arg3311+arg3312+arg3313+arg3314+arg3315+arg3316+arg3317+arg3318+arg3319+arg3320+arg3321+arg3322+arg3323+arg3324+arg3325+arg3326+arg3327+arg3328+arg3329+arg3330+arg3331+arg3332+arg3333+arg3334+arg3335+arg3336+arg3337+arg3338+arg3339+arg3340+arg3341+arg3342+arg3343+arg3344+arg3345+arg3346+arg3347+arg3348+arg3349+arg3350+arg3351+arg3352+arg3353+arg3354+arg3355+arg3356+arg3357+arg3358+arg3359+arg3360+arg3361+arg3362+arg3363+arg3364+arg3365+arg3366+arg3367+arg3368+arg3369+arg3370+arg3371+arg3372+arg3373+arg3374+arg3375+arg3376+arg3377+arg3378+arg3379+arg3380+arg3381+arg3382+arg3383+arg3384+arg3385+arg3386+arg3387+arg3388+arg3389+arg3390+arg3391+arg3392+arg3393+arg3394+arg3395+arg3396+arg3397+arg3398+arg3399+arg3400+arg3401+arg3402+arg3403+arg3404+arg3405+arg3406+arg3407+arg3408+arg3409+arg3410+arg3411+arg3412+arg3413+arg3414+arg3415+arg3416+arg3417+arg3418+arg3419+arg3420+arg3421+arg3422+arg3423+arg3424+arg3425+arg3426+arg3427+arg3428+arg3429+arg3430+arg3431+arg3432+arg3433+arg3434+arg3435+arg3436+arg3437+arg3438+arg3439+arg3440+arg3441+arg3442+arg3443+arg3444+arg3445+arg3446+arg3447+arg3448+arg3449+arg3450+arg3451+arg3452+arg3453+arg3454+arg3455+arg3456+arg3457+arg3458+arg3459+arg3460+arg3461+arg3462+arg3463+arg3464+arg3465+arg3466+arg3467+arg3468+arg3469+arg3470+arg3471+arg3472+arg3473+arg3474+arg3475+arg3476+arg3477+arg3478+arg3479+arg3480+arg3481+arg3482+arg3483+arg3484+arg3485+arg3486+arg3487+arg3488+arg3489+arg3490+arg3491+arg3492+arg3493+arg3494+arg3495+arg3496+arg3497+arg3498+arg3499+arg3500+arg3501+arg3502+arg3503+arg3504+arg3505+arg3506+arg3507+arg3508+arg3509+arg3510+arg3511+arg3512+arg3513+arg3514+arg3515+arg3516+arg3517+arg3518+arg3519+arg3520+arg3521+arg3522+arg3523+arg3524+arg3525+arg3526+arg3527+arg3528+arg3529+arg3530+arg3531+arg3532+arg3533+arg3534+arg3535+arg3536+arg3537+arg3538+arg3539+arg3540+arg3541+arg3542+arg3543+arg3544+arg3545+arg3546+arg3547+arg3548+arg3549+arg3550+arg3551+arg3552+arg3553+arg3554+arg3555+arg3556+arg3557+arg3558+arg3559+arg3560+arg3561+arg3562+arg3563+arg3564+arg3565+arg3566+arg3567+arg3568+arg3569+arg3570+arg3571+arg3572+arg3573+arg3574+arg3575+arg3576+arg3577+arg3578+arg3579+arg3580+arg3581+arg3582+arg3583+arg3584+arg3585+arg3586+arg3587+arg3588+arg3589+arg3590+arg3591+arg3592+arg3593+arg3594+arg3595+arg3596+arg3597+arg3598+arg3599+arg3600+arg3601+arg3602+arg3603+arg3604+arg3605+arg3606+arg3607+arg3608+arg3609+arg3610+arg3611+arg3612+arg3613+arg3614+arg3615+arg3616+arg3617+arg3618+arg3619+arg3620+arg3621+arg3622+arg3623+arg3624+arg3625+arg3626+arg3627+arg3628+arg3629+arg3630+arg3631+arg3632+arg3633+arg3634+arg3635+arg3636+arg3637+arg3638+arg3639+arg3640+arg3641+arg3642+arg3643+arg3644+arg3645+arg3646+arg3647+arg3648+arg3649+arg3650+arg3651+arg3652+arg3653+arg3654+arg3655+arg3656+arg3657+arg3658+arg3659+arg3660+arg3661+arg3662+arg3663+arg3664+arg3665+arg3666+arg3667+arg3668+arg3669+arg3670+arg3671+arg3672+arg3673+arg3674+arg3675+arg3676+arg3677+arg3678+arg3679+arg3680+arg3681+arg3682+arg3683+arg3684+arg3685+arg3686+arg3687+arg3688+arg3689+arg3690+arg3691+arg3692+arg3693+arg3694+arg3695+arg3696+arg3697+arg3698+arg3699+arg3700+arg3701+arg3702+arg3703+arg3704+arg3705+arg3706+arg3707+arg3708+arg3709+arg3710+arg3711+arg3712+arg3713+arg3714+arg3715+arg3716+arg3717+arg3718+arg3719+arg3720+arg3721+arg3722+arg3723+arg3724+arg3725+arg3726+arg3727+arg3728+arg3729+arg3730+arg3731+arg3732+arg3733+arg3734+arg3735+arg3736+arg3737+arg3738+arg3739+arg3740+arg3741+arg3742+arg3743+arg3744+arg3745+arg3746+arg3747+arg3748+arg3749+arg3750+arg3751+arg3752+arg3753+arg3754+arg3755+arg3756+arg3757+arg3758+arg3759+arg3760+arg3761+arg3762+arg3763+arg3764+arg3765+arg3766+arg3767+arg3768+arg3769+arg3770+arg3771+arg3772+arg3773+arg3774+arg3775+arg3776+arg3777+arg3778+arg3779+arg3780+arg3781+arg3782+arg3783+arg3784+arg3785+arg3786+arg3787+arg3788+arg3789+arg3790+arg3791+arg3792+arg3793+arg3794+arg3795+arg3796+arg3797+arg3798+arg3799+arg3800+arg3801+arg3802+arg3803+arg3804+arg3805+arg3806+arg3807+arg3808+arg3809+arg3810+arg3811+arg3812+arg3813+arg3814+arg3815+arg3816+arg3817+arg3818+arg3819+arg3820+arg3821+arg3822+arg3823+arg3824+arg3825+arg3826+arg3827+arg3828+arg3829+arg3830+arg3831+arg3832+arg3833+arg3834+arg3835+arg3836+arg3837+arg3838+arg3839+arg3840+arg3841+arg3842+arg3843+arg3844+arg3845+arg3846+arg3847+arg3848+arg3849+arg3850+arg3851+arg3852+arg3853+arg3854+arg3855+arg3856+arg3857+arg3858+arg3859+arg3860+arg3861+arg3862+arg3863+arg3864+arg3865+arg3866+arg3867+arg3868+arg3869+arg3870+arg3871+arg3872+arg3873+arg3874+arg3875+arg3876+arg3877+arg3878+arg3879+arg3880+arg3881+arg3882+arg3883+arg3884+arg3885+arg3886+arg3887+arg3888+arg3889+arg3890+arg3891+arg3892+arg3893+arg3894+arg3895+arg3896+arg3897+arg3898+arg3899+arg3900+arg3901+arg3902+arg3903+arg3904+arg3905+arg3906+arg3907+arg3908+arg3909+arg3910+arg3911+arg3912+arg3913+arg3914+arg3915+arg3916+arg3917+arg3918+arg3919+arg3920+arg3921+arg3922+arg3923+arg3924+arg3925+arg3926+arg3927+arg3928+arg3929+arg3930+arg3931+arg3932+arg3933+arg3934+arg3935+arg3936+arg3937+arg3938+arg3939+arg3940+arg3941+arg3942+arg3943+arg3944+arg3945+arg3946+arg3947+arg3948+arg3949+arg3950+arg3951+arg3952+arg3953+arg3954+arg3955+arg3956+arg3957+arg3958+arg3959+arg3960+arg3961+arg3962+arg3963+arg3964+arg3965+arg3966+arg3967+arg3968+arg3969+arg3970+arg3971+arg3972+arg3973+arg3974+arg3975+arg3976+arg3977+arg3978+arg3979+arg3980+arg3981+arg3982+arg3983+arg3984+arg3985+arg3986+arg3987+arg3988+arg3989+arg3990+arg3991+arg3992+arg3993+arg3994+arg3995+arg3996+arg3997+arg3998+arg3999+arg4000+arg4001+arg4002+arg4003+arg4004+arg4005+arg4006+arg4007+arg4008+arg4009+arg4010+arg4011+arg4012+arg4013+arg4014+arg4015+arg4016+arg4017+arg4018+arg4019+arg4020+arg4021+arg4022+arg4023+arg4024+arg4025+arg4026+arg4027+arg4028+arg4029+arg4030+arg4031+arg4032+arg4033+arg4034+arg4035+arg4036+arg4037+arg4038+arg4039+arg4040+arg4041+arg4042+arg4043+arg4044+arg4045+arg4046+arg4047+arg4048+arg4049+arg4050+arg4051+arg4052+arg4053+arg4054+arg4055+arg4056+arg4057+arg4058+arg4059+arg4060+arg4061+arg4062+arg4063+arg4064+arg4065+arg4066+arg4067+arg4068+arg4069+arg4070+arg4071+arg4072+arg4073+arg4074+arg4075+arg4076+arg4077+arg4078+arg4079+arg4080+arg4081+arg4082+arg4083+arg4084+arg4085+arg4086+arg4087+arg4088+arg4089+arg4090+arg4091+arg4092+arg4093+arg4094+arg4095+arg4096+arg4097+arg4098+arg4099+arg4100+arg4101+arg4102+arg4103+arg4104+arg4105+arg4106+arg4107+arg4108+arg4109+arg4110+arg4111+arg4112+arg4113+arg4114+arg4115+arg4116+arg4117+arg4118+arg4119+arg4120+arg4121+arg4122+arg4123+arg4124+arg4125+arg4126+arg4127+arg4128+arg4129+arg4130+arg4131+arg4132+arg4133+arg4134+arg4135+arg4136+arg4137+arg4138+arg4139+arg4140+arg4141+arg4142+arg4143+arg4144+arg4145+arg4146+arg4147+arg4148+arg4149+arg4150+arg4151+arg4152+arg4153+arg4154+arg4155+arg4156+arg4157+arg4158+arg4159+arg4160+arg4161+arg4162+arg4163+arg4164+arg4165+arg4166+arg4167+arg4168+arg4169+arg4170+arg4171+arg4172+arg4173+arg4174+arg4175+arg4176+arg4177+arg4178+arg4179+arg4180+arg4181+arg4182+arg4183+arg4184+arg4185+arg4186+arg4187+arg4188+arg4189+arg4190+arg4191+arg4192+arg4193+arg4194+arg4195+arg4196+arg4197+arg4198+arg4199+arg4200+arg4201+arg4202+arg4203+arg4204+arg4205+arg4206+arg4207+arg4208+arg4209+arg4210+arg4211+arg4212+arg4213+arg4214+arg4215+arg4216+arg4217+arg4218+arg4219+arg4220+arg4221+arg4222+arg4223+arg4224+arg4225+arg4226+arg4227+arg4228+arg4229+arg4230+arg4231+arg4232+arg4233+arg4234+arg4235+arg4236+arg4237+arg4238+arg4239+arg4240+arg4241+arg4242+arg4243+arg4244+arg4245+arg4246+arg4247+arg4248+arg4249+arg4250+arg4251+arg4252+arg4253+arg4254+arg4255+arg4256+arg4257+arg4258+arg4259+arg4260+arg4261+arg4262+arg4263+arg4264+arg4265+arg4266+arg4267+arg4268+arg4269+arg4270+arg4271+arg4272+arg4273+arg4274+arg4275+arg4276+arg4277+arg4278+arg4279+arg4280+arg4281+arg4282+arg4283+arg4284+arg4285+arg4286+arg4287+arg4288+arg4289+arg4290+arg4291+arg4292+arg4293+arg4294+arg4295+arg4296+arg4297+arg4298+arg4299+arg4300+arg4301+arg4302+arg4303+arg4304+arg4305+arg4306+arg4307+arg4308+arg4309+arg4310+arg4311+arg4312+arg4313+arg4314+arg4315+arg4316+arg4317+arg4318+arg4319+arg4320+arg4321+arg4322+arg4323+arg4324+arg4325+arg4326+arg4327+arg4328+arg4329+arg4330+arg4331+arg4332+arg4333+arg4334+arg4335+arg4336+arg4337+arg4338+arg4339+arg4340+arg4341+arg4342+arg4343+arg4344+arg4345+arg4346+arg4347+arg4348+arg4349+arg4350+arg4351+arg4352+arg4353+arg4354+arg4355+arg4356+arg4357+arg4358+arg4359+arg4360+arg4361+arg4362+arg4363+arg4364+arg4365+arg4366+arg4367+arg4368+arg4369+arg4370+arg4371+arg4372+arg4373+arg4374+arg4375+arg4376+arg4377+arg4378+arg4379+arg4380+arg4381+arg4382+arg4383+arg4384+arg4385+arg4386+arg4387+arg4388+arg4389+arg4390+arg4391+arg4392+arg4393+arg4394+arg4395+arg4396+arg4397+arg4398+arg4399+arg4400+arg4401+arg4402+arg4403+arg4404+arg4405+arg4406+arg4407+arg4408+arg4409+arg4410+arg4411+arg4412+arg4413+arg4414+arg4415+arg4416+arg4417+arg4418+arg4419+arg4420+arg4421+arg4422+arg4423+arg4424+arg4425+arg4426+arg4427+arg4428+arg4429+arg4430+arg4431+arg4432+arg4433+arg4434+arg4435+arg4436+arg4437+arg4438+arg4439+arg4440+arg4441+arg4442+arg4443+arg4444+arg4445+arg4446+arg4447+arg4448+arg4449+arg4450+arg4451+arg4452+arg4453+arg4454+arg4455+arg4456+arg4457+arg4458+arg4459+arg4460+arg4461+arg4462+arg4463+arg4464+arg4465+arg4466+arg4467+arg4468+arg4469+arg4470+arg4471+arg4472+arg4473+arg4474+arg4475+arg4476+arg4477+arg4478+arg4479+arg4480+arg4481+arg4482+arg4483+arg4484+arg4485+arg4486+arg4487+arg4488+arg4489+arg4490+arg4491+arg4492+arg4493+arg4494+arg4495+arg4496+arg4497+arg4498+arg4499+arg4500+arg4501+arg4502+arg4503+arg4504+arg4505+arg4506+arg4507+arg4508+arg4509+arg4510+arg4511+arg4512+arg4513+arg4514+arg4515+arg4516+arg4517+arg4518+arg4519+arg4520+arg4521+arg4522+arg4523+arg4524+arg4525+arg4526+arg4527+arg4528+arg4529+arg4530+arg4531+arg4532+arg4533+arg4534+arg4535+arg4536+arg4537+arg4538+arg4539+arg4540+arg4541+arg4542+arg4543+arg4544+arg4545+arg4546+arg4547+arg4548+arg4549+arg4550+arg4551+arg4552+arg4553+arg4554+arg4555+arg4556+arg4557+arg4558+arg4559+arg4560+arg4561+arg4562+arg4563+arg4564+arg4565+arg4566+arg4567+arg4568+arg4569+arg4570+arg4571+arg4572+arg4573+arg4574+arg4575+arg4576+arg4577+arg4578+arg4579+arg4580+arg4581+arg4582+arg4583+arg4584+arg4585+arg4586+arg4587+arg4588+arg4589+arg4590+arg4591+arg4592+arg4593+arg4594+arg4595+arg4596+arg4597+arg4598+arg4599+arg4600+arg4601+arg4602+arg4603+arg4604+arg4605+arg4606+arg4607+arg4608+arg4609+arg4610+arg4611+arg4612+arg4613+arg4614+arg4615+arg4616+arg4617+arg4618+arg4619+arg4620+arg4621+arg4622+arg4623+arg4624+arg4625+arg4626+arg4627+arg4628+arg4629+arg4630+arg4631+arg4632+arg4633+arg4634+arg4635+arg4636+arg4637+arg4638+arg4639+arg4640+arg4641+arg4642+arg4643+arg4644+arg4645+arg4646+arg4647+arg4648+arg4649+arg4650+arg4651+arg4652+arg4653+arg4654+arg4655+arg4656+arg4657+arg4658+arg4659+arg4660+arg4661+arg4662+arg4663+arg4664+arg4665+arg4666+arg4667+arg4668+arg4669+arg4670+arg4671+arg4672+arg4673+arg4674+arg4675+arg4676+arg4677+arg4678+arg4679+arg4680+arg4681+arg4682+arg4683+arg4684+arg4685+arg4686+arg4687+arg4688+arg4689+arg4690+arg4691+arg4692+arg4693+arg4694+arg4695+arg4696+arg4697+arg4698+arg4699+arg4700+arg4701+arg4702+arg4703+arg4704+arg4705+arg4706+arg4707+arg4708+arg4709+arg4710+arg4711+arg4712+arg4713+arg4714+arg4715+arg4716+arg4717+arg4718+arg4719+arg4720+arg4721+arg4722+arg4723+arg4724+arg4725+arg4726+arg4727+arg4728+arg4729+arg4730+arg4731+arg4732+arg4733+arg4734+arg4735+arg4736+arg4737+arg4738+arg4739+arg4740+arg4741+arg4742+arg4743+arg4744+arg4745+arg4746+arg4747+arg4748+arg4749+arg4750+arg4751+arg4752+arg4753+arg4754+arg4755+arg4756+arg4757+arg4758+arg4759+arg4760+arg4761+arg4762+arg4763+arg4764+arg4765+arg4766+arg4767+arg4768+arg4769+arg4770+arg4771+arg4772+arg4773+arg4774+arg4775+arg4776+arg4777+arg4778+arg4779+arg4780+arg4781+arg4782+arg4783+arg4784+arg4785+arg4786+arg4787+arg4788+arg4789+arg4790+arg4791+arg4792+arg4793+arg4794+arg4795+arg4796+arg4797+arg4798+arg4799+arg4800+arg4801+arg4802+arg4803+arg4804+arg4805+arg4806+arg4807+arg4808+arg4809+arg4810+arg4811+arg4812+arg4813+arg4814+arg4815+arg4816+arg4817+arg4818+arg4819+arg4820+arg4821+arg4822+arg4823+arg4824+arg4825+arg4826+arg4827+arg4828+arg4829+arg4830+arg4831+arg4832+arg4833+arg4834+arg4835+arg4836+arg4837+arg4838+arg4839+arg4840+arg4841+arg4842+arg4843+arg4844+arg4845+arg4846+arg4847+arg4848+arg4849+arg4850+arg4851+arg4852+arg4853+arg4854+arg4855+arg4856+arg4857+arg4858+arg4859+arg4860+arg4861+arg4862+arg4863+arg4864+arg4865+arg4866+arg4867+arg4868+arg4869+arg4870+arg4871+arg4872+arg4873+arg4874+arg4875+arg4876+arg4877+arg4878+arg4879+arg4880+arg4881+arg4882+arg4883+arg4884+arg4885+arg4886+arg4887+arg4888+arg4889+arg4890+arg4891+arg4892+arg4893+arg4894+arg4895+arg4896+arg4897+arg4898+arg4899+arg4900+arg4901+arg4902+arg4903+arg4904+arg4905+arg4906+arg4907+arg4908+arg4909+arg4910+arg4911+arg4912+arg4913+arg4914+arg4915+arg4916+arg4917+arg4918+arg4919+arg4920+arg4921+arg4922+arg4923+arg4924+arg4925+arg4926+arg4927+arg4928+arg4929+arg4930+arg4931+arg4932+arg4933+arg4934+arg4935+arg4936+arg4937+arg4938+arg4939+arg4940+arg4941+arg4942+arg4943+arg4944+arg4945+arg4946+arg4947+arg4948+arg4949+arg4950+arg4951+arg4952+arg4953+arg4954+arg4955+arg4956+arg4957+arg4958+arg4959+arg4960+arg4961+arg4962+arg4963+arg4964+arg4965+arg4966+arg4967+arg4968+arg4969+arg4970+arg4971+arg4972+arg4973+arg4974+arg4975+arg4976+arg4977+arg4978+arg4979+arg4980+arg4981+arg4982+arg4983+arg4984+arg4985+arg4986+arg4987+arg4988+arg4989+arg4990+arg4991+arg4992+arg4993+arg4994+arg4995+arg4996+arg4997+arg4998+arg4999;}
void testCallFunctionWithHellaArguments3()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> args;
    for (unsigned i = 0; i < 5000; ++i)
        args.append(root->appendNew<Const32Value>(proc, Origin(), 4095 - 5000 + i - 1));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionWithHellaArguments3, B3CCallPtrTag)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    std::unique_ptr<Compilation> compilation = compileProc(proc);
    CHECK(invoke<int>(*compilation) == invoke<int>(*compilation));
    CHECK(invoke<int>(*compilation) == 7967500);
    CHECK(invoke<int>(*compilation) == invoke<int>(*compilation));
    CHECK(invoke<int>(*compilation) == 7967500);
    CHECK(invoke<int>(*compilation) == invoke<int>(*compilation));
    CHECK(invoke<int>(*compilation) == 7967500);
    CHECK(invoke<int>(*compilation) == invoke<int>(*compilation));
    CHECK(invoke<int>(*compilation) == 7967500);
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

static double simpleFunctionDouble(double a, double b)
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
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunctionDouble, B3CCallPtrTag)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    CHECK(compileAndRun<double>(proc, a, b) == a + b);
}

static float simpleFunctionFloat(float a, float b)
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
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunctionFloat, B3CCallPtrTag)),
            floatValue1,
            floatValue2));

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), a + b));
}

static double functionWithHellaDoubleArguments(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k, double l, double m, double n, double o, double p, double q, double r, double s, double t, double u, double v, double w, double x, double y, double z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionWithHellaDoubleArguments, B3CCallPtrTag)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    CHECK(compileAndRun<double>(proc) == functionWithHellaDoubleArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

static float functionWithHellaFloatArguments(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p, float q, float r, float s, float t, float u, float v, float w, float x, float y, float z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionWithHellaFloatArguments, B3CCallPtrTag)));
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
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(simpleFunction, B3CCallPtrTag)),
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
