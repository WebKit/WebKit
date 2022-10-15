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

static const char* const dmbIsh = "dmb      ish";
static const char* const dmbIshst = "dmb      ishst";

void testBitAndSExt32(int32_t value, int64_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, SExt32, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
            root->appendNew<Const64Value>(proc, Origin(), mask)));

    CHECK(compileAndRun<int64_t>(proc, value) == (static_cast<int64_t>(value) & mask));
}

void testBasicSelect()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 43, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 43, 642462, 32533) == 32533);
}

void testSelectTest()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 0, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 0, 642462, 32533) == 32533);
}

void testSelectCompareDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, -1.0, 1.0, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42.5, 42.51, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, PNaN, 0.0, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 42.51, 42.5, 642462, 32533) == 32533);
    CHECK(invoke<intptr_t>(*code, 42.52, 42.52, 524978245, 352) == 352);
}

template<B3::Opcode opcode>
void testSelectCompareFloat(float a, float b, bool (*operation)(float, float))
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
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, opcode, Origin(),
                floatValue1,
                floatValue2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), 42, -5), operation(a, b) ? 42 : -5));
}

void testSelectCompareFloat(float a, float b)
{
    testSelectCompareFloat<Equal>(a, b, [](float a, float b) -> bool { return a == b; });
    testSelectCompareFloat<NotEqual>(a, b, [](float a, float b) -> bool { return a != b; });
    testSelectCompareFloat<LessThan>(a, b, [](float a, float b) -> bool { return a < b; });
    testSelectCompareFloat<GreaterThan>(a, b, [](float a, float b) -> bool { return a > b; });
    testSelectCompareFloat<LessEqual>(a, b, [](float a, float b) -> bool { return a <= b; });
    testSelectCompareFloat<GreaterEqual>(a, b, [](float a, float b) -> bool { return a >= b; });
    testSelectCompareFloat<EqualOrUnordered>(a, b, [](float a, float b) -> bool { return a != a || b != b || a == b; });
}

template<B3::Opcode opcode>
void testSelectCompareFloatToDouble(float a, float b, bool (*operation)(float, float))
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* doubleValue1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* doubleValue2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, opcode, Origin(),
                doubleValue1,
                doubleValue2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), 42, -5), operation(a, b) ? 42 : -5));
}

void testSelectCompareFloatToDouble(float a, float b)
{
    testSelectCompareFloatToDouble<Equal>(a, b, [](float a, float b) -> bool { return a == b; });
    testSelectCompareFloatToDouble<NotEqual>(a, b, [](float a, float b) -> bool { return a != b; });
    testSelectCompareFloatToDouble<LessThan>(a, b, [](float a, float b) -> bool { return a < b; });
    testSelectCompareFloatToDouble<GreaterThan>(a, b, [](float a, float b) -> bool { return a > b; });
    testSelectCompareFloatToDouble<LessEqual>(a, b, [](float a, float b) -> bool { return a <= b; });
    testSelectCompareFloatToDouble<GreaterEqual>(a, b, [](float a, float b) -> bool { return a >= b; });
    testSelectCompareFloatToDouble<EqualOrUnordered>(a, b, [](float a, float b) -> bool { return a != a || b != b || a == b; });
}

void testSelectDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    auto code = compileProc(proc);
    CHECK(invoke<double>(*code, 42, 1.5, 2.6) == 1.5);
    CHECK(invoke<double>(*code, 42, 642462.7, 32533.8) == 642462.7);
    CHECK(invoke<double>(*code, 43, 1.9, 2.0) == 2.0);
    CHECK(invoke<double>(*code, 43, 642462.1, 32533.2) == 32533.2);
}

void testSelectDoubleTest()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    auto code = compileProc(proc);
    CHECK(invoke<double>(*code, 42, 1.5, 2.6) == 1.5);
    CHECK(invoke<double>(*code, 42, 642462.7, 32533.8) == 642462.7);
    CHECK(invoke<double>(*code, 0, 1.9, 2.0) == 2.0);
    CHECK(invoke<double>(*code, 0, 642462.1, 32533.2) == 32533.2);
}

void testSelectDoubleCompareDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3)));

    auto code = compileProc(proc);
    CHECK(invoke<double>(*code, -1.0, 1.0, 1.1, 2.2) == 1.1);
    CHECK(invoke<double>(*code, 42.5, 42.51, 642462.3, 32533.4) == 642462.3);
    CHECK(invoke<double>(*code, PNaN, 0.0, 1.5, 2.6) == 2.6);
    CHECK(invoke<double>(*code, 42.51, 42.5, 642462.7, 32533.8) == 32533.8);
    CHECK(invoke<double>(*code, 42.52, 42.52, 524978245.9, 352.0) == 352.0);
}

void testSelectDoubleCompareFloat(float a, float b)
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
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                floatValue1,
                floatValue2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    CHECK(isIdentical(compileAndRun<double>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), 42.1, -M_PI), a < b ? 42.1 : -M_PI));
}

void testSelectFloatCompareFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* argument3int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    Value* argument4int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* floatValue3 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument3int32);
    Value* floatValue4 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument4int32);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                floatValue1,
                floatValue2),
            floatValue3,
            floatValue4));

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), bitwise_cast<int32_t>(1.1f), bitwise_cast<int32_t>(-42.f)), a < b ? 1.1f : -42.f));
}


template<B3::Opcode opcode>
void testSelectDoubleCompareDouble(bool (*operation)(double, double))
{
    { // Compare arguments and selected arguments are all different.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);
        Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3);

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, opcode, Origin(),
                    arg0,
                    arg1),
                arg2,
                arg3));
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, -66.5), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. "thenCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);
        Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3);

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, -66.5), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. "elseCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);
        Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3);

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, -66.5), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. Both cases are live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);
        Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3);

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, -66.5), expected));
            }
        }
    }
    { // The left argument is the same as the "elseCase" argument.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, opcode, Origin(),
                    arg0,
                    arg1),
                arg2,
                arg0));
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : left.value;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, left.value), expected));
            }
        }
    }
    { // The left argument is the same as the "elseCase" argument. "thenCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2);

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg0);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<double>()) {
            for (auto& right : floatingPointOperands<double>()) {
                double expected = operation(left.value, right.value) ? 42.5 : left.value;
                CHECK(isIdentical(invoke<double>(*code, left.value, right.value, 42.5, left.value), expected));
            }
        }
    }
}

void testSelectDoubleCompareDoubleWithAliasing()
{
    testSelectDoubleCompareDouble<Equal>([](double a, double b) -> bool { return a == b; });
    testSelectDoubleCompareDouble<NotEqual>([](double a, double b) -> bool { return a != b; });
    testSelectDoubleCompareDouble<LessThan>([](double a, double b) -> bool { return a < b; });
    testSelectDoubleCompareDouble<GreaterThan>([](double a, double b) -> bool { return a > b; });
    testSelectDoubleCompareDouble<LessEqual>([](double a, double b) -> bool { return a <= b; });
    testSelectDoubleCompareDouble<GreaterEqual>([](double a, double b) -> bool { return a >= b; });
    testSelectDoubleCompareDouble<EqualOrUnordered>([](double a, double b) -> bool { return a != a || b != b || a == b; });
}

template<B3::Opcode opcode>
void testSelectFloatCompareFloat(bool (*operation)(float, float))
{
    { // Compare arguments and selected arguments are all different.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));
        Value* arg3 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, opcode, Origin(),
                    arg0,
                    arg1),
                arg2,
                arg3));
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(-66.5f)), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. "thenCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));
        Value* arg3 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(-66.5f)), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. "elseCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));
        Value* arg3 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(-66.5f)), expected));
            }
        }
    }
    { // Compare arguments and selected arguments are all different. Both cases are live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));
        Value* arg3 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)));

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg3);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : -66.5;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(-66.5f)), expected));
            }
        }
    }
    { // The left argument is the same as the "elseCase" argument.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, opcode, Origin(),
                    arg0,
                    arg1),
                arg2,
                arg0));
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : left.value;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(left.value)), expected));
            }
        }
    }
    { // The left argument is the same as the "elseCase" argument. "thenCase" is live after operation.
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg0 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
        Value* arg1 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        Value* arg2 = root->appendNew<Value>(proc, BitwiseCast, Origin(),
            root->appendNew<Value>(proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

        Value* result = root->appendNew<Value>(proc, Select, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), arg0, arg1),
            arg2,
            arg0);

        PatchpointValue* keepValuesLive = root->appendNew<PatchpointValue>(proc, Void, Origin());
        keepValuesLive->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        keepValuesLive->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);

        for (auto& left : floatingPointOperands<float>()) {
            for (auto& right : floatingPointOperands<float>()) {
                float expected = operation(left.value, right.value) ? 42.5 : left.value;
                CHECK(isIdentical(invoke<float>(*code, bitwise_cast<int32_t>(left.value), bitwise_cast<int32_t>(right.value), bitwise_cast<int32_t>(42.5f), bitwise_cast<int32_t>(left.value)), expected));
            }
        }
    }
}

void testSelectFloatCompareFloatWithAliasing()
{
    testSelectFloatCompareFloat<Equal>([](float a, float b) -> bool { return a == b; });
    testSelectFloatCompareFloat<NotEqual>([](float a, float b) -> bool { return a != b; });
    testSelectFloatCompareFloat<LessThan>([](float a, float b) -> bool { return a < b; });
    testSelectFloatCompareFloat<GreaterThan>([](float a, float b) -> bool { return a > b; });
    testSelectFloatCompareFloat<LessEqual>([](float a, float b) -> bool { return a <= b; });
    testSelectFloatCompareFloat<GreaterEqual>([](float a, float b) -> bool { return a >= b; });
    testSelectFloatCompareFloat<EqualOrUnordered>([](float a, float b) -> bool { return a != a || b != b || a == b; });
}

void testSelectFold(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ConstPtrValue>(proc, Origin(), value),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, 1, 2) == (value == 42 ? 1 : 2));
    CHECK(invoke<intptr_t>(*code, 642462, 32533) == (value == 42 ? 642462 : 32533));
}

void testSelectInvert()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<Value>(
                    proc, NotEqual, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                    root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
                root->appendNew<Const32Value>(proc, Origin(), 0)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 43, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 43, 642462, 32533) == 32533);
}

void testCheckSelect()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();

    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, BitAnd, Origin(),
                    root->appendNew<Value>(
                        proc, Trunc, Origin(),
                        root->appendNew<ArgumentRegValue>(
                            proc, Origin(), GPRInfo::argumentGPR0)),
                    root->appendNew<Const32Value>(proc, Origin(), 0xff)),
                root->appendNew<ConstPtrValue>(proc, Origin(), -42),
                root->appendNew<ConstPtrValue>(proc, Origin(), 35)),
            root->appendNew<ConstPtrValue>(proc, Origin(), 42)));
    unsigned generationCount = 0;
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            generationCount++;
            jit.move(CCallHelpers::TrustedImm32(666), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(generationCount == 1);
    CHECK(invoke<int>(*code, true) == 0);
    CHECK(invoke<int>(*code, false) == 666);
}

void testCheckSelectCheckSelect()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();

    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, BitAnd, Origin(),
                    root->appendNew<Value>(
                        proc, Trunc, Origin(),
                        root->appendNew<ArgumentRegValue>(
                            proc, Origin(), GPRInfo::argumentGPR0)),
                    root->appendNew<Const32Value>(proc, Origin(), 0xff)),
                root->appendNew<ConstPtrValue>(proc, Origin(), -42),
                root->appendNew<ConstPtrValue>(proc, Origin(), 35)),
            root->appendNew<ConstPtrValue>(proc, Origin(), 42)));

    unsigned generationCount = 0;
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            generationCount++;
            jit.move(CCallHelpers::TrustedImm32(666), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    CheckValue* check2 = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Select, Origin(),
                root->appendNew<Value>(
                    proc, BitAnd, Origin(),
                    root->appendNew<Value>(
                        proc, Trunc, Origin(),
                        root->appendNew<ArgumentRegValue>(
                            proc, Origin(), GPRInfo::argumentGPR1)),
                    root->appendNew<Const32Value>(proc, Origin(), 0xff)),
                root->appendNew<ConstPtrValue>(proc, Origin(), -43),
                root->appendNew<ConstPtrValue>(proc, Origin(), 36)),
            root->appendNew<ConstPtrValue>(proc, Origin(), 43)));

    unsigned generationCount2 = 0;
    check2->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            generationCount2++;
            jit.move(CCallHelpers::TrustedImm32(667), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(generationCount == 1);
    CHECK(generationCount2 == 1);
    CHECK(invoke<int>(*code, true, true) == 0);
    CHECK(invoke<int>(*code, false, true) == 666);
    CHECK(invoke<int>(*code, true, false) == 667);
}

void testCheckSelectAndCSE()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();

    auto* selectValue = root->appendNew<Value>(
        proc, Select, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(
                    proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), 0xff)),
        root->appendNew<ConstPtrValue>(proc, Origin(), -42),
        root->appendNew<ConstPtrValue>(proc, Origin(), 35));

    auto* constant = root->appendNew<ConstPtrValue>(proc, Origin(), 42);
    auto* addValue = root->appendNew<Value>(proc, Add, Origin(), selectValue, constant);

    CheckValue* check = root->appendNew<CheckValue>(proc, Check, Origin(), addValue);
    unsigned generationCount = 0;
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            generationCount++;
            jit.move(CCallHelpers::TrustedImm32(666), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    auto* addValue2 = root->appendNew<Value>(proc, Add, Origin(), selectValue, constant);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), addValue, addValue2));

    auto code = compileProc(proc);
    CHECK(generationCount == 1);
    CHECK(invoke<int>(*code, true) == 0);
    CHECK(invoke<int>(*code, false) == 666);
}

double b3Pow(double x, int y)
{
    if (y < 0 || y > 1000)
        return pow(x, y);
    double result = 1;
    while (y) {
        if (y & 1)
            result *= x;
        x *= x;
        y >>= 1;
    }
    return result;
}

void testPowDoubleByIntegerLoop(double xOperand, int32_t yOperand)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* x = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* y = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    auto result = powDoubleInt32(proc, root, Origin(), x, y);
    BasicBlock* continuation = result.first;
    continuation->appendNewControlValue(proc, Return, Origin(), result.second);

    CHECK(isIdentical(compileAndRun<double>(proc, xOperand, yOperand), b3Pow(xOperand, yOperand)));
}

void testTruncOrHigh()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const64Value>(proc, Origin(), 0x100000000))));

    int64_t value = 0x123456781234;
    CHECK(compileAndRun<int>(proc, value) == 0x56781234);
}

void testTruncOrLow()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const64Value>(proc, Origin(), 0x1000000))));

    int64_t value = 0x123456781234;
    CHECK(compileAndRun<int>(proc, value) == 0x57781234);
}

void testBitAndOrHigh()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const64Value>(proc, Origin(), 0x8)),
            root->appendNew<Const64Value>(proc, Origin(), 0x777777777777)));

    int64_t value = 0x123456781234;
    CHECK(compileAndRun<int64_t>(proc, value) == 0x123456701234ll);
}

void testBitAndOrLow()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const64Value>(proc, Origin(), 0x1)),
            root->appendNew<Const64Value>(proc, Origin(), 0x777777777777)));

    int64_t value = 0x123456781234;
    CHECK(compileAndRun<int64_t>(proc, value) == 0x123456701235ll);
}

void testBranch64Equal(int64_t left, int64_t right)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    bool trueResult = true;
    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            thenCase->appendNew<ConstPtrValue>(proc, Origin(), &trueResult)));

    bool elseResult = false;
    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            elseCase->appendNew<ConstPtrValue>(proc, Origin(), &elseResult)));

    CHECK(compileAndRun<bool>(proc, left, right) == (left == right));
}

void testBranch64EqualImm(int64_t left, int64_t right)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ConstPtrValue>(proc, Origin(), right);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    bool trueResult = true;
    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            thenCase->appendNew<ConstPtrValue>(proc, Origin(), &trueResult)));

    bool elseResult = false;
    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            elseCase->appendNew<ConstPtrValue>(proc, Origin(), &elseResult)));

    CHECK(compileAndRun<bool>(proc, left) == (left == right));
}

void testBranch64EqualMem(int64_t left, int64_t right)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<MemoryValue>(
        proc, Load, pointerType(), Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    bool trueResult = true;
    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            thenCase->appendNew<ConstPtrValue>(proc, Origin(), &trueResult)));

    bool elseResult = false;
    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            elseCase->appendNew<ConstPtrValue>(proc, Origin(), &elseResult)));

    CHECK(compileAndRun<bool>(proc, &left, right) == (left == right));
}

void testBranch64EqualMemImm(int64_t left, int64_t right)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<MemoryValue>(
        proc, Load, pointerType(), Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<ConstPtrValue>(proc, Origin(), right);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    bool trueResult = true;
    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            thenCase->appendNew<ConstPtrValue>(proc, Origin(), &trueResult)));

    bool elseResult = false;
    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            elseCase->appendNew<ConstPtrValue>(proc, Origin(), &elseResult)));

    CHECK(compileAndRun<bool>(proc, &left) == (left == right));
}

void testStore8Load8Z(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    int8_t byte;
    Value* ptr = root->appendNew<ConstPtrValue>(proc, Origin(), &byte);

    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        ptr);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(proc, Load8Z, Origin(), ptr));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<uint8_t>(value));
}

void testStore16Load16Z(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    int16_t byte;
    Value* ptr = root->appendNew<ConstPtrValue>(proc, Origin(), &byte);

    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        ptr);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(proc, Load16Z, Origin(), ptr));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<uint16_t>(value));
}

static void testSShrShl32(int32_t value, int32_t sshrAmount, int32_t shlAmount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), shlAmount)),
            root->appendNew<Const32Value>(proc, Origin(), sshrAmount)));

    CHECK(
        compileAndRun<int32_t>(proc, value)
        == ((value << (shlAmount & 31)) >> (sshrAmount & 31)));
}

static void testSShrShl64(int64_t value, int32_t sshrAmount, int32_t shlAmount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const32Value>(proc, Origin(), shlAmount)),
            root->appendNew<Const32Value>(proc, Origin(), sshrAmount)));

    CHECK(
        compileAndRun<int64_t>(proc, value)
        == ((value << (shlAmount & 63)) >> (sshrAmount & 63)));
}

void testTrivialInfiniteLoop()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* loop = proc.addBlock();
    root->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loop));
    loop->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loop));

    compileProc(proc);
}

void testFoldPathEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenBlock = proc.addBlock();
    BasicBlock* elseBlock = proc.addBlock();

    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    root->appendNewControlValue(
        proc, Branch, Origin(), arg, FrequentedBlock(thenBlock), FrequentedBlock(elseBlock));

    thenBlock->appendNewControlValue(
        proc, Return, Origin(),
        thenBlock->appendNew<Value>(
            proc, Equal, Origin(), arg, thenBlock->appendNew<ConstPtrValue>(proc, Origin(), 0)));

    elseBlock->appendNewControlValue(
        proc, Return, Origin(),
        elseBlock->appendNew<Value>(
            proc, Equal, Origin(), arg, elseBlock->appendNew<ConstPtrValue>(proc, Origin(), 0)));

    auto code = compileProc(proc);
    CHECK(invoke<intptr_t>(*code, 0) == 1);
    CHECK(invoke<intptr_t>(*code, 1) == 0);
    CHECK(invoke<intptr_t>(*code, 42) == 0);
}

void testLShiftSelf32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Shl, Origin(), arg, arg));

    auto code = compileProc(proc);

    auto check = [&] (int32_t value) {
        CHECK(invoke<int32_t>(*code, value) == value << (value & 31));
    };

    check(0);
    check(1);
    check(31);
    check(32);
}

void testRShiftSelf32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), arg, arg));

    auto code = compileProc(proc);

    auto check = [&] (int32_t value) {
        CHECK(invoke<int32_t>(*code, value) == value >> (value & 31));
    };

    check(0);
    check(1);
    check(31);
    check(32);
}

void testURShiftSelf32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, ZShr, Origin(), arg, arg));

    auto code = compileProc(proc);

    auto check = [&] (uint32_t value) {
        CHECK(invoke<uint32_t>(*code, value) == value >> (value & 31));
    };

    check(0);
    check(1);
    check(31);
    check(32);
}

void testLShiftSelf64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(), arg, root->appendNew<Value>(proc, Trunc, Origin(), arg)));

    auto code = compileProc(proc);

    auto check = [&] (int64_t value) {
        CHECK(invoke<int64_t>(*code, value) == value << (value & 63));
    };

    check(0);
    check(1);
    check(31);
    check(32);
    check(63);
    check(64);
}

void testRShiftSelf64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(), arg, root->appendNew<Value>(proc, Trunc, Origin(), arg)));

    auto code = compileProc(proc);

    auto check = [&] (int64_t value) {
        CHECK(invoke<int64_t>(*code, value) == value >> (value & 63));
    };

    check(0);
    check(1);
    check(31);
    check(32);
    check(63);
    check(64);
}

void testURShiftSelf64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(), arg, root->appendNew<Value>(proc, Trunc, Origin(), arg)));

    auto code = compileProc(proc);

    auto check = [&] (uint64_t value) {
        CHECK(invoke<uint64_t>(*code, value) == value >> (value & 63));
    };

    check(0);
    check(1);
    check(31);
    check(32);
    check(63);
    check(64);
}

void testPatchpointDoubleRegs()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Double, Origin());
    patchpoint->append(arg, ValueRep(FPRInfo::fpRegT0));
    patchpoint->resultConstraints = { ValueRep(FPRInfo::fpRegT0) };

    unsigned numCalls = 0;
    patchpoint->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            numCalls++;
        });

    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    auto code = compileProc(proc);
    CHECK(numCalls == 1);
    CHECK(invoke<double>(*code, 42.5) == 42.5);
}

void testSpillDefSmallerThanUse()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    // Move32.
    Value* arg32 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg64 = root->appendNew<Value>(proc, ZExt32, Origin(), arg32);

    // Make sure arg64 is on the stack.
    PatchpointValue* forceSpill = root->appendNew<PatchpointValue>(proc, Int64, Origin());
    RegisterSetBuilder clobberSet = RegisterSetBuilder::allGPRs();
    clobberSet.exclude(RegisterSetBuilder::stackRegisters());
    clobberSet.exclude(RegisterSetBuilder::reservedHardwareRegisters());
    clobberSet.remove(GPRInfo::returnValueGPR); // Force the return value for aliasing below.
    forceSpill->clobberLate(clobberSet);
    forceSpill->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.xor64(params[0].gpr(), params[0].gpr());
        });

    // On x86, Sub admit an address for any operand. If it uses the stack, the top bits must be zero.
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), forceSpill, arg64);
    root->appendNewControlValue(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK(invoke<int64_t>(*code, 0xffffffff00000000) == 0);
}

void testSpillUseLargerThanDef()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    RegisterSetBuilder clobberSet = RegisterSetBuilder::allGPRs();
    clobberSet.exclude(RegisterSetBuilder::stackRegisters());
    clobberSet.exclude(RegisterSetBuilder::reservedHardwareRegisters());

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            condition),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    Value* truncated = thenCase->appendNew<Value>(proc, ZExt32, Origin(),
        thenCase->appendNew<Value>(proc, Trunc, Origin(), argument));
    UpsilonValue* thenResult = thenCase->appendNew<UpsilonValue>(proc, Origin(), truncated);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    UpsilonValue* elseResult = elseCase->appendNew<UpsilonValue>(proc, Origin(), argument);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    for (unsigned i = 0; i < 100; ++i) {
        PatchpointValue* preventTailDuplication = tail->appendNew<PatchpointValue>(proc, Void, Origin());
        preventTailDuplication->clobberLate(clobberSet);
        preventTailDuplication->setGenerator([] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    PatchpointValue* forceSpill = tail->appendNew<PatchpointValue>(proc, Void, Origin());
    forceSpill->clobberLate(clobberSet);
    forceSpill->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            clobberSet.buildAndValidate().forEach([&] (Reg reg) {
                jit.move(CCallHelpers::TrustedImm64(0xffffffffffffffff), reg.gpr());
            });
        });

    Value* phi = tail->appendNew<Value>(proc, Phi, Int64, Origin());
    thenResult->setPhi(phi);
    elseResult->setPhi(phi);
    tail->appendNewControlValue(proc, Return, Origin(), phi);

    auto code = compileProc(proc);
    CHECK(invoke<uint64_t>(*code, 1, 0xffffffff00000000) == 0);
    CHECK(invoke<uint64_t>(*code, 0, 0xffffffff00000000) == 0xffffffff00000000);

    // A second time since the previous run is still on the stack.
    CHECK(invoke<uint64_t>(*code, 1, 0xffffffff00000000) == 0);

}

void testLateRegister()
{
    Procedure proc;

    if (!proc.optLevel()) {
        // FIXME: Make O0 handle such situations:
        // https://bugs.webkit.org/show_bug.cgi?id=194633
        return;
    }

    BasicBlock* root = proc.addBlock();

    // This works by making all but 1 register be input to the first patchpoint as LateRegister.
    // The other 1 register is just a regular Register input. We assert our result is the regular
    // register input. There would be no other way for the register allocator to arrange things
    // because LateRegister interferes with the result.
    // Then, the second patchpoint takes the result of the first as an argument and asks for
    // it in a register that was a LateRegister. This is to incentivize the register allocator
    // to use that LateRegister as the result for the first patchpoint. But of course it can not do that.
    // So it must issue a mov after the first patchpoint from the first's result into the second's input.

    RegisterSetBuilder regs = RegisterSetBuilder::allGPRs();
    regs.exclude(RegisterSetBuilder::stackRegisters());
    regs.exclude(RegisterSetBuilder::reservedHardwareRegisters());
    Vector<Value*> lateUseArgs;
    unsigned result = 0;
    for (GPRReg reg = CCallHelpers::firstRegister(); reg <= CCallHelpers::lastRegister(); reg = CCallHelpers::nextRegister(reg)) {
        if (!regs.buildAndValidate().contains(reg, IgnoreVectors))
            continue;
        result++;
        if (reg == GPRInfo::regT0)
            continue;
        Value* value = root->appendNew<Const64Value>(proc, Origin(), 1);
        lateUseArgs.append(value);
    }
    Value* regularUse = root->appendNew<Const64Value>(proc, Origin(), 1);
    PatchpointValue* firstPatchpoint = root->appendNew<PatchpointValue>(proc, Int64, Origin());
    {
        unsigned i = 0;
        for (GPRReg reg = CCallHelpers::firstRegister(); reg <= CCallHelpers::lastRegister(); reg = CCallHelpers::nextRegister(reg)) {
            if (!regs.buildAndValidate().contains(reg, IgnoreVectors))
                continue;
            if (reg == GPRInfo::regT0)
                continue;
            Value* value = lateUseArgs[i++];
            firstPatchpoint->append(value, ValueRep::lateReg(reg));
        }
        firstPatchpoint->append(regularUse, ValueRep::reg(GPRInfo::regT0));
    }

    firstPatchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params[0].gpr() == GPRInfo::regT0);
            // Note that regT0 should also start off as 1, so we're implicitly starting our add with 1, which is also an argument.
            unsigned skipped = 0;
            for (unsigned i = 1; i < params.size(); i++) {
                if (params[i].gpr() == params[0].gpr()) {
                    skipped = i;
                    continue;
                }
                jit.add64(params[i].gpr(), params[0].gpr());
            }
            CHECK(!!skipped);
        });

    PatchpointValue* secondPatchpoint = root->appendNew<PatchpointValue>(proc, Int64, Origin());
    secondPatchpoint->append(firstPatchpoint, ValueRep::reg(GPRInfo::regT1));
    secondPatchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params[1].gpr() == GPRInfo::regT1);
            jit.nop();
            jit.nop();
            jit.move(params[1].gpr(), params[0].gpr());
            jit.nop();
            jit.nop();
        });
    root->appendNewControlValue(proc, Return, Origin(), secondPatchpoint);

    auto code = compileProc(proc);
    CHECK(invoke<uint64_t>(*code) == result);
}

JSC_DECLARE_JIT_OPERATION(interpreterPrint, void, (Vector<intptr_t>* stream, intptr_t value));
JSC_DEFINE_JIT_OPERATION(interpreterPrint, void, (Vector<intptr_t>* stream, intptr_t value))
{
    stream->append(value);
}

void testInterpreter()
{
    // This implements a silly interpreter to test building custom switch statements using
    // Patchpoint.

    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* dispatch = proc.addBlock();
    BasicBlock* addToDataPointer = proc.addBlock();
    BasicBlock* addToCodePointer = proc.addBlock();
    BasicBlock* addToCodePointerTaken = proc.addBlock();
    BasicBlock* addToCodePointerNotTaken = proc.addBlock();
    BasicBlock* addToData = proc.addBlock();
    BasicBlock* print = proc.addBlock();
    BasicBlock* stop = proc.addBlock();

    Variable* dataPointer = proc.addVariable(pointerType());
    Variable* codePointer = proc.addVariable(pointerType());

    root->appendNew<VariableValue>(
        proc, Set, Origin(), dataPointer,
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* context = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    root->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(dispatch));

    // NOTE: It's totally valid for this patchpoint to be tail-duplicated.
    Value* codePointerValue =
        dispatch->appendNew<VariableValue>(proc, B3::Get, Origin(), codePointer);
    Value* opcode = dispatch->appendNew<MemoryValue>(
        proc, Load, pointerType(), Origin(), codePointerValue);
    PatchpointValue* polyJump = dispatch->appendNew<PatchpointValue>(proc, Void, Origin());
    polyJump->effects = Effects();
    polyJump->effects.terminal = true;
    polyJump->appendSomeRegister(opcode);
    polyJump->clobber(RegisterSetBuilder::macroClobberedRegisters());
    polyJump->numGPScratchRegisters = 2;
    dispatch->appendSuccessor(FrequentedBlock(addToDataPointer));
    dispatch->appendSuccessor(FrequentedBlock(addToCodePointer));
    dispatch->appendSuccessor(FrequentedBlock(addToData));
    dispatch->appendSuccessor(FrequentedBlock(print));
    dispatch->appendSuccessor(FrequentedBlock(stop));

    // Our "opcodes".
    static constexpr intptr_t AddDP = 0;
    static constexpr intptr_t AddCP = 1;
    static constexpr intptr_t Add = 2;
    static constexpr intptr_t Print = 3;
    static constexpr intptr_t Stop = 4;

    polyJump->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();

            CodePtr<JSSwitchPtrTag>* jumpTable = bitwise_cast<CodePtr<JSSwitchPtrTag>*>(
                params.proc().addDataSection(sizeof(CodePtr<JSSwitchPtrTag>) * labels.size()));

            GPRReg scratch = params.gpScratch(0);

            jit.move(CCallHelpers::TrustedImmPtr(jumpTable), scratch);
            jit.load64(CCallHelpers::BaseIndex(scratch, params[0].gpr(), CCallHelpers::ScalePtr), scratch);
            jit.farJump(scratch, JSSwitchPtrTag);

            jit.addLinkTask(
                [&, jumpTable, labels] (LinkBuffer& linkBuffer) {
                    for (unsigned i = labels.size(); i--;)
                        jumpTable[i] = linkBuffer.locationOf<JSSwitchPtrTag>(*labels[i]);
                });
        });

    // AddDP <operand>: adds <operand> to DP.
    codePointerValue =
        addToDataPointer->appendNew<VariableValue>(proc, B3::Get, Origin(), codePointer);
    addToDataPointer->appendNew<VariableValue>(
        proc, Set, Origin(), dataPointer,
        addToDataPointer->appendNew<Value>(
            proc, B3::Add, Origin(),
            addToDataPointer->appendNew<VariableValue>(proc, B3::Get, Origin(), dataPointer),
            addToDataPointer->appendNew<Value>(
                proc, Mul, Origin(),
                addToDataPointer->appendNew<MemoryValue>(
                    proc, Load, pointerType(), Origin(), codePointerValue, static_cast<int32_t>(sizeof(intptr_t))),
                addToDataPointer->appendIntConstant(
                    proc, Origin(), pointerType(), sizeof(intptr_t)))));
    addToDataPointer->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        addToDataPointer->appendNew<Value>(
            proc, B3::Add, Origin(), codePointerValue,
            addToDataPointer->appendIntConstant(
                proc, Origin(), pointerType(), sizeof(intptr_t) * 2)));
    addToDataPointer->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(dispatch));

    // AddCP <operand>: adds <operand> to CP if the current value at DP is non-zero, otherwise
    // falls through normally.
    codePointerValue =
        addToCodePointer->appendNew<VariableValue>(proc, B3::Get, Origin(), codePointer);
    Value* dataPointerValue =
        addToCodePointer->appendNew<VariableValue>(proc, B3::Get, Origin(), dataPointer);
    addToCodePointer->appendNewControlValue(
        proc, Branch, Origin(),
        addToCodePointer->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(), dataPointerValue),
        FrequentedBlock(addToCodePointerTaken), FrequentedBlock(addToCodePointerNotTaken));
    addToCodePointerTaken->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        addToCodePointerTaken->appendNew<Value>(
            proc, B3::Add, Origin(), codePointerValue,
            addToCodePointerTaken->appendNew<Value>(
                proc, Mul, Origin(),
                addToCodePointerTaken->appendNew<MemoryValue>(
                    proc, Load, pointerType(), Origin(), codePointerValue, static_cast<int32_t>(sizeof(intptr_t))),
                addToCodePointerTaken->appendIntConstant(
                    proc, Origin(), pointerType(), sizeof(intptr_t)))));
    addToCodePointerTaken->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(dispatch));
    addToCodePointerNotTaken->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        addToCodePointerNotTaken->appendNew<Value>(
            proc, B3::Add, Origin(), codePointerValue,
            addToCodePointerNotTaken->appendIntConstant(
                proc, Origin(), pointerType(), sizeof(intptr_t) * 2)));
    addToCodePointerNotTaken->appendNewControlValue(
        proc, Jump, Origin(), FrequentedBlock(dispatch));

    // Add <operand>: adds <operand> to the slot pointed to by DP.
    codePointerValue = addToData->appendNew<VariableValue>(proc, B3::Get, Origin(), codePointer);
    dataPointerValue = addToData->appendNew<VariableValue>(proc, B3::Get, Origin(), dataPointer);
    addToData->appendNew<MemoryValue>(
        proc, Store, Origin(),
        addToData->appendNew<Value>(
            proc, B3::Add, Origin(),
            addToData->appendNew<MemoryValue>(
                proc, Load, pointerType(), Origin(), dataPointerValue),
            addToData->appendNew<MemoryValue>(
                proc, Load, pointerType(), Origin(), codePointerValue, static_cast<int32_t>(sizeof(intptr_t)))),
        dataPointerValue);
    addToData->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        addToData->appendNew<Value>(
            proc, B3::Add, Origin(), codePointerValue,
            addToData->appendIntConstant(proc, Origin(), pointerType(), sizeof(intptr_t) * 2)));
    addToData->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(dispatch));

    // Print: "prints" the value pointed to by DP. What this actually means is that the value is
    // appended to the stream vector by the interpreterPrint function.
    codePointerValue = print->appendNew<VariableValue>(proc, B3::Get, Origin(), codePointer);
    dataPointerValue = print->appendNew<VariableValue>(proc, B3::Get, Origin(), dataPointer);
    print->appendNew<CCallValue>(
        proc, Void, Origin(),
        print->appendNew<ConstPtrValue>(
            proc, Origin(), tagCFunction<OperationPtrTag>(interpreterPrint)),
        context,
        print->appendNew<MemoryValue>(proc, Load, pointerType(), Origin(), dataPointerValue));
    print->appendNew<VariableValue>(
        proc, Set, Origin(), codePointer,
        print->appendNew<Value>(
            proc, B3::Add, Origin(), codePointerValue,
            print->appendIntConstant(proc, Origin(), pointerType(), sizeof(intptr_t))));
    print->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(dispatch));

    // Stop: returns.
    stop->appendNewControlValue(
        proc, Return, Origin(),
        stop->appendIntConstant(proc, Origin(), pointerType(), 0));

    auto interpreter = compileProc(proc);

    Vector<uintptr_t> data;
    Vector<uintptr_t> code;
    Vector<uintptr_t> stream;

    data.append(1);
    data.append(0);

    if (shouldBeVerbose(proc))
        dataLog("data = ", listDump(data), "\n");

    // We'll write a program that prints the numbers 1..100.
    // We expect DP to point at #0.
    code.append(AddCP);
    code.append(6); // go to loop body

    // Loop re-entry:
    // We expect DP to point at #1 and for #1 to be offset by -100.
    code.append(Add);
    code.append(100);

    code.append(AddDP);
    code.append(-1);

    // Loop header:
    // We expect DP to point at #0.
    code.append(AddDP);
    code.append(1);

    code.append(Add);
    code.append(1);

    code.append(Print);

    code.append(Add);
    code.append(-100);

    // We want to stop if it's zero and continue if it's non-zero. AddCP takes the branch if it's
    // non-zero.
    code.append(AddCP);
    code.append(-11); // go to loop re-entry.

    code.append(Stop);

    if (shouldBeVerbose(proc))
        dataLog("code = ", listDump(code), "\n");

    CHECK(!invoke<intptr_t>(*interpreter, data.data(), code.data(), &stream));

    CHECK(stream.size() == 100);
    for (unsigned i = 0; i < 100; ++i)
        CHECK(stream[i] == i + 1);

    if (shouldBeVerbose(proc))
        dataLog("stream = ", listDump(stream), "\n");
}

void testReduceStrengthCheckBottomUseInAnotherBlock()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;

    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();

    CheckValue* check = one->appendNew<CheckValue>(
        proc, Check, Origin(), one->appendNew<Const32Value>(proc, Origin(), 1));
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            jit.move(CCallHelpers::TrustedImm32(666), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    Value* arg = one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    one->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(two));

    check = two->appendNew<CheckValue>(
        proc, CheckAdd, Origin(), arg,
        two->appendNew<ConstPtrValue>(proc, Origin(), 1));
    check->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            CHECK(!"Should not execute");
        });
    two->appendNewControlValue(proc, Return, Origin(), check);

    proc.resetReachability();
    reduceStrength(proc);
}

void testResetReachabilityDanglingReference()
{
    Procedure proc;

    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();

    UpsilonValue* upsilon = one->appendNew<UpsilonValue>(
        proc, Origin(), one->appendNew<Const32Value>(proc, Origin(), 42));
    one->appendNewControlValue(proc, Oops, Origin());

    Value* phi = two->appendNew<Value>(proc, Phi, Int32, Origin());
    upsilon->setPhi(phi);
    two->appendNewControlValue(proc, Oops, Origin());

    proc.resetReachability();
    validate(proc);
}

void testEntrySwitchSimple()
{
    Procedure proc;
    proc.setNumEntrypoints(3);

    BasicBlock* root = proc.addBlock();
    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();
    BasicBlock* three = proc.addBlock();

    root->appendNew<Value>(proc, EntrySwitch, Origin());
    root->appendSuccessor(FrequentedBlock(one));
    root->appendSuccessor(FrequentedBlock(two));
    root->appendSuccessor(FrequentedBlock(three));

    one->appendNew<Value>(
        proc, Return, Origin(),
        one->appendNew<Value>(
            proc, Add, Origin(),
            one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    two->appendNew<Value>(
        proc, Return, Origin(),
        two->appendNew<Value>(
            proc, Sub, Origin(),
            two->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            two->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    three->appendNew<Value>(
        proc, Return, Origin(),
        three->appendNew<Value>(
            proc, Mul, Origin(),
            three->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            three->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    prepareForGeneration(proc);

    CCallHelpers jit;
    generate(proc, jit);
    LinkBuffer linkBuffer(jit, nullptr);
    CodeLocationLabel<JITCompilationPtrTag> labelOne = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(0));
    CodeLocationLabel<JITCompilationPtrTag> labelTwo = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(1));
    CodeLocationLabel<JITCompilationPtrTag> labelThree = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(2));

    MacroAssemblerCodeRef<JITCompilationPtrTag> codeRef = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "testb3 compilation");

    CHECK(invoke<int>(labelOne, 1, 2) == 3);
    CHECK(invoke<int>(labelTwo, 1, 2) == -1);
    CHECK(invoke<int>(labelThree, 1, 2) == 2);
    CHECK(invoke<int>(labelOne, -1, 2) == 1);
    CHECK(invoke<int>(labelTwo, -1, 2) == -3);
    CHECK(invoke<int>(labelThree, -1, 2) == -2);
}

void testEntrySwitchNoEntrySwitch()
{
    Procedure proc;
    proc.setNumEntrypoints(3);

    BasicBlock* root = proc.addBlock();

    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    prepareForGeneration(proc);

    CCallHelpers jit;
    generate(proc, jit);
    LinkBuffer linkBuffer(jit, nullptr);
    CodeLocationLabel<JITCompilationPtrTag> labelOne = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(0));
    CodeLocationLabel<JITCompilationPtrTag> labelTwo = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(1));
    CodeLocationLabel<JITCompilationPtrTag> labelThree = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(2));

    MacroAssemblerCodeRef<JITCompilationPtrTag> codeRef = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "testb3 compilation");

    CHECK_EQ(invoke<int>(labelOne, 1, 2), 3);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2), 3);
    CHECK_EQ(invoke<int>(labelThree, 1, 2), 3);
    CHECK_EQ(invoke<int>(labelOne, -1, 2), 1);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2), 1);
    CHECK_EQ(invoke<int>(labelThree, -1, 2), 1);
}

void testEntrySwitchWithCommonPaths()
{
    Procedure proc;
    proc.setNumEntrypoints(3);

    BasicBlock* root = proc.addBlock();
    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();
    BasicBlock* three = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    root->appendNew<Value>(proc, EntrySwitch, Origin());
    root->appendSuccessor(FrequentedBlock(one));
    root->appendSuccessor(FrequentedBlock(two));
    root->appendSuccessor(FrequentedBlock(three));

    UpsilonValue* upsilonOne = one->appendNew<UpsilonValue>(
        proc, Origin(),
        one->appendNew<Value>(
            proc, Add, Origin(),
            one->appendNew<Value>(
                proc, Trunc, Origin(),
                one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            one->appendNew<Value>(
                proc, Trunc, Origin(),
                one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    one->appendNew<Value>(proc, Jump, Origin());
    one->setSuccessors(FrequentedBlock(end));

    UpsilonValue* upsilonTwo = two->appendNew<UpsilonValue>(
        proc, Origin(),
        two->appendNew<Value>(
            proc, Sub, Origin(),
            two->appendNew<Value>(
                proc, Trunc, Origin(),
                two->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            two->appendNew<Value>(
                proc, Trunc, Origin(),
                two->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    two->appendNew<Value>(proc, Jump, Origin());
    two->setSuccessors(FrequentedBlock(end));

    UpsilonValue* upsilonThree = three->appendNew<UpsilonValue>(
        proc, Origin(),
        three->appendNew<Value>(
            proc, Mul, Origin(),
            three->appendNew<Value>(
                proc, Trunc, Origin(),
                three->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            three->appendNew<Value>(
                proc, Trunc, Origin(),
                three->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    three->appendNew<Value>(proc, Jump, Origin());
    three->setSuccessors(FrequentedBlock(end));

    Value* phi = end->appendNew<Value>(proc, Phi, Int32, Origin());
    upsilonOne->setPhi(phi);
    upsilonTwo->setPhi(phi);
    upsilonThree->setPhi(phi);

    end->appendNew<Value>(
        proc, Return, Origin(),
        end->appendNew<Value>(
            proc, chill(Mod), Origin(),
            phi, end->appendNew<Value>(
                proc, Trunc, Origin(),
                end->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2))));

    prepareForGeneration(proc);

    CCallHelpers jit;
    generate(proc, jit);
    LinkBuffer linkBuffer(jit, nullptr);
    CodeLocationLabel<JITCompilationPtrTag> labelOne = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(0));
    CodeLocationLabel<JITCompilationPtrTag> labelTwo = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(1));
    CodeLocationLabel<JITCompilationPtrTag> labelThree = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(2));

    MacroAssemblerCodeRef<JITCompilationPtrTag> codeRef = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "testb3 compilation");

    CHECK_EQ(invoke<int>(labelOne, 1, 2, 10), 3);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 10), -1);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 10), 2);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 10), 1);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 10), -3);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 10), -2);
    CHECK_EQ(invoke<int>(labelOne, 1, 2, 2), 1);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 2), -1);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 2), 0);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 2), 1);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 2), -1);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 2), 0);
    CHECK_EQ(invoke<int>(labelOne, 1, 2, 0), 0);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 0), 0);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 0), 0);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 0), 0);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 0), 0);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 0), 0);
}

void testEntrySwitchWithCommonPathsAndNonTrivialEntrypoint()
{
    Procedure proc;
    proc.setNumEntrypoints(3);

    BasicBlock* root = proc.addBlock();
    BasicBlock* negate = proc.addBlock();
    BasicBlock* dispatch = proc.addBlock();
    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();
    BasicBlock* three = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    UpsilonValue* upsilonBase = root->appendNew<UpsilonValue>(
        proc, Origin(), root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    root->appendNew<Value>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0xff)));
    root->setSuccessors(FrequentedBlock(negate), FrequentedBlock(dispatch));

    UpsilonValue* upsilonNegate = negate->appendNew<UpsilonValue>(
        proc, Origin(),
        negate->appendNew<Value>(
            proc, Neg, Origin(),
            negate->appendNew<Value>(
                proc, Trunc, Origin(),
                negate->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));
    negate->appendNew<Value>(proc, Jump, Origin());
    negate->setSuccessors(FrequentedBlock(dispatch));

    Value* arg0 = dispatch->appendNew<Value>(proc, Phi, Int32, Origin());
    upsilonBase->setPhi(arg0);
    upsilonNegate->setPhi(arg0);
    dispatch->appendNew<Value>(proc, EntrySwitch, Origin());
    dispatch->appendSuccessor(FrequentedBlock(one));
    dispatch->appendSuccessor(FrequentedBlock(two));
    dispatch->appendSuccessor(FrequentedBlock(three));

    UpsilonValue* upsilonOne = one->appendNew<UpsilonValue>(
        proc, Origin(),
        one->appendNew<Value>(
            proc, Add, Origin(),
            arg0, one->appendNew<Value>(
                proc, Trunc, Origin(),
                one->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    one->appendNew<Value>(proc, Jump, Origin());
    one->setSuccessors(FrequentedBlock(end));

    UpsilonValue* upsilonTwo = two->appendNew<UpsilonValue>(
        proc, Origin(),
        two->appendNew<Value>(
            proc, Sub, Origin(),
            arg0, two->appendNew<Value>(
                proc, Trunc, Origin(),
                two->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    two->appendNew<Value>(proc, Jump, Origin());
    two->setSuccessors(FrequentedBlock(end));

    UpsilonValue* upsilonThree = three->appendNew<UpsilonValue>(
        proc, Origin(),
        three->appendNew<Value>(
            proc, Mul, Origin(),
            arg0, three->appendNew<Value>(
                proc, Trunc, Origin(),
                three->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));
    three->appendNew<Value>(proc, Jump, Origin());
    three->setSuccessors(FrequentedBlock(end));

    Value* phi = end->appendNew<Value>(proc, Phi, Int32, Origin());
    upsilonOne->setPhi(phi);
    upsilonTwo->setPhi(phi);
    upsilonThree->setPhi(phi);

    end->appendNew<Value>(
        proc, Return, Origin(),
        end->appendNew<Value>(
            proc, chill(Mod), Origin(),
            phi, end->appendNew<Value>(
                proc, Trunc, Origin(),
                end->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2))));

    prepareForGeneration(proc);

    CCallHelpers jit;
    generate(proc, jit);
    LinkBuffer linkBuffer(jit, nullptr);
    CodeLocationLabel<JITCompilationPtrTag> labelOne = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(0));
    CodeLocationLabel<JITCompilationPtrTag> labelTwo = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(1));
    CodeLocationLabel<JITCompilationPtrTag> labelThree = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(2));

    MacroAssemblerCodeRef<JITCompilationPtrTag> codeRef = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "testb3 compilation");

    CHECK_EQ(invoke<int>(labelOne, 1, 2, 10, false), 3);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 10, false), -1);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 10, false), 2);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 10, false), 1);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 10, false), -3);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 10, false), -2);
    CHECK_EQ(invoke<int>(labelOne, 1, 2, 10, true), 1);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 10, true), -3);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 10, true), -2);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 10, true), 3);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 10, true), -1);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 10, true), 2);
    CHECK_EQ(invoke<int>(labelOne, 1, 2, 2, false), 1);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 2, false), -1);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 2, false), 0);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 2, false), 1);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 2, false), -1);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 2, false), 0);
    CHECK_EQ(invoke<int>(labelOne, 1, 2, 0, false), 0);
    CHECK_EQ(invoke<int>(labelTwo, 1, 2, 0, false), 0);
    CHECK_EQ(invoke<int>(labelThree, 1, 2, 0, false), 0);
    CHECK_EQ(invoke<int>(labelOne, -1, 2, 0, false), 0);
    CHECK_EQ(invoke<int>(labelTwo, -1, 2, 0, false), 0);
    CHECK_EQ(invoke<int>(labelThree, -1, 2, 0, false), 0);
}

void testEntrySwitchLoop()
{
    // This is a completely absurd use of EntrySwitch, where it impacts the loop condition. This
    // should cause duplication of either nearly the entire Procedure. At time of writing, we ended
    // up duplicating all of it, which is fine. It's important to test this case, to make sure that
    // the duplication algorithm can handle interesting control flow.

    Procedure proc;
    proc.setNumEntrypoints(2);

    BasicBlock* root = proc.addBlock();
    BasicBlock* loopHeader = proc.addBlock();
    BasicBlock* loopFooter = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    UpsilonValue* initialValue = root->appendNew<UpsilonValue>(
        proc, Origin(), root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    root->appendNew<Value>(proc, Jump, Origin());
    root->setSuccessors(loopHeader);

    Value* valueInLoop = loopHeader->appendNew<Value>(proc, Phi, Int32, Origin());
    initialValue->setPhi(valueInLoop);
    Value* newValue = loopHeader->appendNew<Value>(
        proc, Add, Origin(), valueInLoop,
        loopHeader->appendNew<Const32Value>(proc, Origin(), 1));
    loopHeader->appendNew<Value>(proc, EntrySwitch, Origin());
    loopHeader->appendSuccessor(end);
    loopHeader->appendSuccessor(loopFooter);

    loopFooter->appendNew<UpsilonValue>(proc, Origin(), newValue, valueInLoop);
    loopFooter->appendNew<Value>(
        proc, Branch, Origin(),
        loopFooter->appendNew<Value>(
            proc, LessThan, Origin(), newValue,
            loopFooter->appendNew<Const32Value>(proc, Origin(), 100)));
    loopFooter->setSuccessors(loopHeader, end);

    end->appendNew<Value>(proc, Return, Origin(), newValue);

    prepareForGeneration(proc);

    CCallHelpers jit;
    generate(proc, jit);
    LinkBuffer linkBuffer(jit, nullptr);
    CodeLocationLabel<JITCompilationPtrTag> labelOne = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(0));
    CodeLocationLabel<JITCompilationPtrTag> labelTwo = linkBuffer.locationOf<JITCompilationPtrTag>(proc.code().entrypointLabel(1));

    MacroAssemblerCodeRef<JITCompilationPtrTag> codeRef = FINALIZE_CODE(linkBuffer, JITCompilationPtrTag, "testb3 compilation");

    CHECK(invoke<int>(labelOne, 0) == 1);
    CHECK(invoke<int>(labelOne, 42) == 43);
    CHECK(invoke<int>(labelOne, 1000) == 1001);

    CHECK(invoke<int>(labelTwo, 0) == 100);
    CHECK(invoke<int>(labelTwo, 42) == 100);
    CHECK(invoke<int>(labelTwo, 1000) == 1001);
}

void testSomeEarlyRegister()
{
    auto run = [&] (bool succeed) {
        Procedure proc;
    
        BasicBlock* root = proc.addBlock();
    
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->resultConstraints = { ValueRep::reg(GPRInfo::returnValueGPR) };
        bool ranFirstPatchpoint = false;
        patchpoint->setGenerator(
            [&] (CCallHelpers&, const StackmapGenerationParams& params) {
                CHECK(params[0].gpr() == GPRInfo::returnValueGPR);
                ranFirstPatchpoint = true;
            });
    
        Value* arg = patchpoint;
    
        patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->appendSomeRegister(arg);
        if (succeed)
            patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister };
        bool ranSecondPatchpoint = false;
        unsigned optLevel = proc.optLevel();
        patchpoint->setGenerator(
            [&] (CCallHelpers&, const StackmapGenerationParams& params) {
                if (succeed)
                    CHECK(params[0].gpr() != params[1].gpr());
                else if (optLevel > 1)
                    CHECK(params[0].gpr() == params[1].gpr());
                ranSecondPatchpoint = true;
            });
    
        root->appendNew<Value>(proc, Return, Origin(), patchpoint);
    
        compileProc(proc);
        CHECK(ranFirstPatchpoint);
        CHECK(ranSecondPatchpoint);
    };

    run(true);
    run(false);
}

void testBranchBitAndImmFusion(
    B3::Opcode valueModifier, Type valueType, int64_t constant,
    Air::Opcode expectedOpcode, Air::Arg::Kind firstKind)
{
    // Currently this test should pass on all CPUs. But some CPUs may not support this fused
    // instruction. It's OK to skip this test on those CPUs.

    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* one = proc.addBlock();
    BasicBlock* two = proc.addBlock();

    Value* left = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    if (valueModifier != Identity) {
        if (MemoryValue::accepts(valueModifier))
            left = root->appendNew<MemoryValue>(proc, valueModifier, valueType, Origin(), left);
        else
            left = root->appendNew<Value>(proc, valueModifier, valueType, Origin(), left);
    }

    root->appendNew<Value>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(), left,
            root->appendIntConstant(proc, Origin(), valueType, constant)));
    root->setSuccessors(FrequentedBlock(one), FrequentedBlock(two));

    one->appendNew<Value>(proc, Oops, Origin());
    two->appendNew<Value>(proc, Oops, Origin());

    lowerToAirForTesting(proc);

    // The first basic block must end in a BranchTest64(resCond, tmp, bitImm).
    Air::Inst terminal = proc.code()[0]->last();
    CHECK_EQ(terminal.kind.opcode, expectedOpcode);
    CHECK_EQ(terminal.args[0].kind(), Air::Arg::ResCond);
    CHECK_EQ(terminal.args[1].kind(), firstKind);
    CHECK(terminal.args[2].kind() == Air::Arg::BitImm || terminal.args[2].kind() == Air::Arg::BitImm64);
}

void testTerminalPatchpointThatNeedsToBeSpilled()
{
    // This is a unit test for how FTL's heap allocation fast paths behave.
    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* success = proc.addBlock();
    BasicBlock* slowPath = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->effects.terminal = true;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());

    root->appendSuccessor(success);
    root->appendSuccessor(FrequentedBlock(slowPath, FrequencyClass::Rare));

    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(CCallHelpers::TrustedImm32(42), params[0].gpr());
        
            CCallHelpers::Jump jumpToSuccess;
            if (!params.fallsThroughToSuccessor(0))
                jumpToSuccess = jit.jump();
        
            Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();
        
            params.addLatePath(
                [=] (CCallHelpers& jit) {
                    if (jumpToSuccess.isSet())
                        jumpToSuccess.linkTo(*labels[0], &jit);
                });
        });

    Vector<Value*> args;
    {
        RegisterSetBuilder fillAllGPRsSet = proc.mutableGPRs();
        for (unsigned i = 0; i < fillAllGPRsSet.numberOfSetRegisters(); i++)
            args.append(success->appendNew<Const32Value>(proc, Origin(), i));
    }

    {
        // Now force all values into every available register.
        PatchpointValue* p = success->appendNew<PatchpointValue>(proc, Void, Origin());
        for (Value* v : args)
            p->append(v, ValueRep::SomeRegister);
        p->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    {
        // Now require the original patchpoint to be materialized into a register.
        PatchpointValue* p = success->appendNew<PatchpointValue>(proc, Void, Origin());
        p->append(patchpoint, ValueRep::SomeRegister);
        p->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    success->appendNew<Value>(proc, Return, Origin(), success->appendNew<Const32Value>(proc, Origin(), 10));

    slowPath->appendNew<Value>(proc, Return, Origin(), slowPath->appendNew<Const32Value>(proc, Origin(), 20));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code), 10);
}

void testTerminalPatchpointThatNeedsToBeSpilled2()
{
    // This is a unit test for how FTL's heap allocation fast paths behave.
    Procedure proc;

    // FIXME: Air O0/O1 allocator can't handle such programs. We rely on WasmAirIRGenerator
    // to not use any such constructs where the register allocator is cornered in such
    // a way.
    // https://bugs.webkit.org/show_bug.cgi?id=194633
    if (proc.optLevel() < 2)
        return;

    BasicBlock* root = proc.addBlock();
    BasicBlock* one = proc.addBlock();
    BasicBlock* success = proc.addBlock();
    BasicBlock* slowPath = proc.addBlock();

    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));

    root->appendNew<Value>(
        proc, Branch, Origin(), arg);
    root->appendSuccessor(one);
    root->appendSuccessor(FrequentedBlock(slowPath, FrequencyClass::Rare));

    PatchpointValue* patchpoint = one->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->effects.terminal = true;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->append(arg, ValueRep::SomeRegister);

    one->appendSuccessor(success);
    one->appendSuccessor(FrequentedBlock(slowPath, FrequencyClass::Rare));

    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(CCallHelpers::TrustedImm32(666), params[0].gpr());
            auto goToFastPath = jit.branch32(CCallHelpers::Equal, params[1].gpr(), CCallHelpers::TrustedImm32(42));
            auto jumpToSlow = jit.jump();
        
            // Make sure the asserts here pass.
            params.fallsThroughToSuccessor(0);
            params.fallsThroughToSuccessor(1);

            Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();
        
            params.addLatePath(
                [=] (CCallHelpers& jit) {
                    goToFastPath.linkTo(*labels[0], &jit);
                    jumpToSlow.linkTo(*labels[1], &jit);
                });
        });

    Vector<Value*> args;
    {
        RegisterSetBuilder fillAllGPRsSet = proc.mutableGPRs();
        for (unsigned i = 0; i < fillAllGPRsSet.numberOfSetRegisters(); i++)
            args.append(success->appendNew<Const32Value>(proc, Origin(), i));
    }

    {
        // Now force all values into every available register.
        PatchpointValue* p = success->appendNew<PatchpointValue>(proc, Void, Origin());
        for (Value* v : args)
            p->append(v, ValueRep::SomeRegister);
        p->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    {
        // Now require the original patchpoint to be materialized into a register.
        PatchpointValue* p = success->appendNew<PatchpointValue>(proc, Void, Origin());
        p->append(patchpoint, ValueRep::SomeRegister);
        p->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    success->appendNew<Value>(proc, Return, Origin(), patchpoint);

    slowPath->appendNew<Value>(proc, Return, Origin(), arg);

    auto original1 = Options::maxB3TailDupBlockSize();
    auto original2 = Options::maxB3TailDupBlockSuccessors();

    // Tail duplication will break the critical edge we're trying to test because it
    // will clone the slowPath block for both edges to it!
    Options::maxB3TailDupBlockSize() = 0;
    Options::maxB3TailDupBlockSuccessors() = 0;

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code, 1), 1);
    CHECK_EQ(invoke<int>(*code, 0), 0);
    CHECK_EQ(invoke<int>(*code, 42), 666);

    Options::maxB3TailDupBlockSize() = original1;
    Options::maxB3TailDupBlockSuccessors() = original2;
}

void testPatchpointTerminalReturnValue(bool successIsRare)
{
    // This is a unit test for how FTL's heap allocation fast paths behave.
    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* success = proc.addBlock();
    BasicBlock* slowPath = proc.addBlock();
    BasicBlock* continuation = proc.addBlock();

    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->effects.terminal = true;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());

    if (successIsRare) {
        root->appendSuccessor(FrequentedBlock(success, FrequencyClass::Rare));
        root->appendSuccessor(slowPath);
    } else {
        root->appendSuccessor(success);
        root->appendSuccessor(FrequentedBlock(slowPath, FrequencyClass::Rare));
    }

    patchpoint->appendSomeRegister(arg);

    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
        
            CCallHelpers::Jump jumpToSlow =
                jit.branch32(CCallHelpers::Above, params[1].gpr(), CCallHelpers::TrustedImm32(42));
        
            jit.add32(CCallHelpers::TrustedImm32(31), params[1].gpr(), params[0].gpr());
        
            CCallHelpers::Jump jumpToSuccess;
            if (!params.fallsThroughToSuccessor(0))
                jumpToSuccess = jit.jump();
        
            Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();
        
            params.addLatePath(
                [=] (CCallHelpers& jit) {
                    jumpToSlow.linkTo(*labels[1], &jit);
                    if (jumpToSuccess.isSet())
                        jumpToSuccess.linkTo(*labels[0], &jit);
                });
        });

    UpsilonValue* successUpsilon = success->appendNew<UpsilonValue>(proc, Origin(), patchpoint);
    success->appendNew<Value>(proc, Jump, Origin());
    success->setSuccessors(continuation);

    UpsilonValue* slowPathUpsilon = slowPath->appendNew<UpsilonValue>(
        proc, Origin(), slowPath->appendNew<Const32Value>(proc, Origin(), 666));
    slowPath->appendNew<Value>(proc, Jump, Origin());
    slowPath->setSuccessors(continuation);

    Value* phi = continuation->appendNew<Value>(proc, Phi, Int32, Origin());
    successUpsilon->setPhi(phi);
    slowPathUpsilon->setPhi(phi);
    continuation->appendNew<Value>(proc, Return, Origin(), phi);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code, 0), 31);
    CHECK_EQ(invoke<int>(*code, 1), 32);
    CHECK_EQ(invoke<int>(*code, 41), 72);
    CHECK_EQ(invoke<int>(*code, 42), 73);
    CHECK_EQ(invoke<int>(*code, 43), 666);
    CHECK_EQ(invoke<int>(*code, -1), 666);
}

void testMemoryFence()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();

    root->appendNew<FenceValue>(proc, Origin());
    root->appendNew<Value>(proc, Return, Origin(), root->appendIntConstant(proc, Origin(), Int32, 42));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code), 42);
    if (isX86())
        checkUsesInstruction(*code, "lock orl $0x0, (%rsp)");
    if (isARM64())
        checkUsesInstruction(*code, dmbIsh);
    checkDoesNotUseInstruction(*code, "mfence");
    checkDoesNotUseInstruction(*code, dmbIshst);
}

void testStoreFence()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();

    root->appendNew<FenceValue>(proc, Origin(), HeapRange::top(), HeapRange());
    root->appendNew<Value>(proc, Return, Origin(), root->appendIntConstant(proc, Origin(), Int32, 42));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code), 42);
    checkDoesNotUseInstruction(*code, "lock");
    checkDoesNotUseInstruction(*code, "mfence");
    if (isARM64())
        checkUsesInstruction(*code, dmbIshst);
}

void testLoadFence()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();

    root->appendNew<FenceValue>(proc, Origin(), HeapRange(), HeapRange::top());
    root->appendNew<Value>(proc, Return, Origin(), root->appendIntConstant(proc, Origin(), Int32, 42));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code), 42);
    checkDoesNotUseInstruction(*code, "lock");
    checkDoesNotUseInstruction(*code, "mfence");
    if (isARM64())
        checkUsesInstruction(*code, dmbIsh);
    checkDoesNotUseInstruction(*code, dmbIshst);
}

void testTrappingLoad()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    MemoryValue* value = root->appendNew<MemoryValue>(
        proc, trapping(Load), Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), &x));
    Effects expectedEffects;
    expectedEffects.exitsSideways = true;
    expectedEffects.controlDependent= true;
    expectedEffects.reads = HeapRange::top();
    CHECK_EQ(value->range(), HeapRange::top());
    CHECK_EQ(value->effects(), expectedEffects);
    value->setRange(HeapRange(0));
    CHECK_EQ(value->range(), HeapRange(0));
    CHECK_EQ(value->effects(), expectedEffects); // We still reads top!
    root->appendNew<Value>(proc, Return, Origin(), value);
    CHECK_EQ(compileAndRun<int>(proc), 42);
    unsigned trapsCount = 0;
    for (Air::BasicBlock* block : proc.code()) {
        for (Air::Inst& inst : *block) {
            if (inst.kind.effects)
                trapsCount++;
        }
    }
    CHECK_EQ(trapsCount, 1u);
}

void testTrappingStore()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    MemoryValue* value = root->appendNew<MemoryValue>(
        proc, trapping(Store), Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 111),
        root->appendNew<ConstPtrValue>(proc, Origin(), &x), 0);
    Effects expectedEffects;
    expectedEffects.exitsSideways = true;
    expectedEffects.controlDependent= true;
    expectedEffects.reads = HeapRange::top();
    expectedEffects.writes = HeapRange::top();
    CHECK_EQ(value->range(), HeapRange::top());
    CHECK_EQ(value->effects(), expectedEffects);
    value->setRange(HeapRange(0));
    CHECK_EQ(value->range(), HeapRange(0));
    expectedEffects.writes = HeapRange(0);
    CHECK_EQ(value->effects(), expectedEffects); // We still reads top!
    root->appendNew<Value>(proc, Return, Origin());
    compileAndRun<int>(proc);
    CHECK_EQ(x, 111);
    unsigned trapsCount = 0;
    for (Air::BasicBlock* block : proc.code()) {
        for (Air::Inst& inst : *block) {
            if (inst.kind.effects)
                trapsCount++;
        }
    }
    CHECK_EQ(trapsCount, 1u);
}

void testTrappingLoadAddStore()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    ConstPtrValue* ptr = root->appendNew<ConstPtrValue>(proc, Origin(), &x);
    root->appendNew<MemoryValue>(
        proc, trapping(Store), Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, trapping(Load), Int32, Origin(), ptr),
            root->appendNew<Const32Value>(proc, Origin(), 3)),
        ptr, 0);
    root->appendNew<Value>(proc, Return, Origin());
    compileAndRun<int>(proc);
    CHECK_EQ(x, 45);
    bool traps = false;
    for (Air::BasicBlock* block : proc.code()) {
        for (Air::Inst& inst : *block) {
            if (inst.kind.effects)
                traps = true;
        }
    }
    CHECK(traps);
}

void testTrappingLoadDCE()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    root->appendNew<MemoryValue>(
        proc, trapping(Load), Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), &x));
    root->appendNew<Value>(proc, Return, Origin());
    compileAndRun<int>(proc);
    unsigned trapsCount = 0;
    for (Air::BasicBlock* block : proc.code()) {
        for (Air::Inst& inst : *block) {
            if (inst.kind.effects)
                trapsCount++;
        }
    }
    CHECK_EQ(trapsCount, 1u);
}

void testTrappingStoreElimination()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    Value* ptr = root->appendNew<ConstPtrValue>(proc, Origin(), &x);
    root->appendNew<MemoryValue>(
        proc, trapping(Store), Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 43),
        ptr);
    root->appendNew<MemoryValue>(
        proc, trapping(Store), Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 44),
        ptr);
    root->appendNew<Value>(proc, Return, Origin());
    // We'll look at the values after compiling
    proc.code().forcePreservationOfB3Origins();
    compileAndRun<int>(proc);
    unsigned storeCount = 0;
    for (Value* value : proc.values()) {
        if (isStore(value->opcode()))
            storeCount++;
    }
    CHECK_EQ(storeCount, 2u);
}

void testMoveConstants()
{
    auto check = [] (Procedure& proc) {
        proc.resetReachability();
    
        if (shouldBeVerbose(proc)) {
            dataLog("IR before:\n");
            dataLog(proc);
        }
    
        moveConstants(proc);
    
        if (shouldBeVerbose(proc)) {
            dataLog("IR after:\n");
            dataLog(proc);
        }
    
        UseCounts useCounts(proc);
        unsigned count = 0;
        for (Value* value : proc.values()) {
            if (useCounts.numUses(value) && value->hasInt64())
                count++;
        }
    
        if (count == 1)
            return;
    
        crashLock.lock();
        dataLog("Fail in testMoveConstants: got more than one Const64:\n");
        dataLog(proc);
        CRASH();
    };

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* a = root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(), 
            root->appendNew<ConstPtrValue>(proc, Origin(), 0x123412341234));
        Value* b = root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0x123412341334));
        root->appendNew<CCallValue>(proc, Void, Origin(), a, b);
        root->appendNew<Value>(proc, Return, Origin());
        check(proc);
    }

    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* x = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* a = root->appendNew<Value>(
            proc, Add, Origin(), x, root->appendNew<ConstPtrValue>(proc, Origin(), 0x123412341234));
        Value* b = root->appendNew<Value>(
            proc, Add, Origin(), x, root->appendNew<ConstPtrValue>(proc, Origin(), -0x123412341234));
        root->appendNew<CCallValue>(proc, Void, Origin(), a, b);
        root->appendNew<Value>(proc, Return, Origin());
        check(proc);
    }
}

extern "C" {
static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(testMoveConstantsWithLargeOffsetsFunc, double, (double));
}
JSC_DEFINE_JIT_OPERATION(testMoveConstantsWithLargeOffsetsFunc, double, (double a))
{
    return a;
}

void testMoveConstantsWithLargeOffsets()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* result = root->appendNew<ConstDoubleValue>(proc, Origin(), 0);
    double rhs = 0;
    for (size_t i = 0; i < 4100; i++) {
        rhs += i;
        Value* callResult = root->appendNew<CCallValue>(proc, Double, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(testMoveConstantsWithLargeOffsetsFunc)),
            root->appendNew<ConstDoubleValue>(proc, Origin(), i));
        result = root->appendNew<Value>(proc, Add, Origin(), result, callResult);
    }
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<double>(proc), rhs);
}

void testPCOriginMapDoesntInsertNops()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    CCallHelpers::Label watchpointLabel;

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            watchpointLabel = jit.watchpointLabel();
        });

    patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            CCallHelpers::Label labelIgnoringWatchpoints = jit.labelIgnoringWatchpoints();

            CHECK(watchpointLabel == labelIgnoringWatchpoints);
        });

    root->appendNew<Value>(proc, Return, Origin());

    compileProc(proc);
}

void addSShrShTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
    RUN(testSShrShl32(42, 24, 24));
    RUN(testSShrShl32(-42, 24, 24));
    RUN(testSShrShl32(4200, 24, 24));
    RUN(testSShrShl32(-4200, 24, 24));
    RUN(testSShrShl32(4200000, 24, 24));
    RUN(testSShrShl32(-4200000, 24, 24));
    
    RUN(testSShrShl32(42, 16, 16));
    RUN(testSShrShl32(-42, 16, 16));
    RUN(testSShrShl32(4200, 16, 16));
    RUN(testSShrShl32(-4200, 16, 16));
    RUN(testSShrShl32(4200000, 16, 16));
    RUN(testSShrShl32(-4200000, 16, 16));
    
    RUN(testSShrShl32(42, 8, 8));
    RUN(testSShrShl32(-42, 8, 8));
    RUN(testSShrShl32(4200, 8, 8));
    RUN(testSShrShl32(-4200, 8, 8));
    RUN(testSShrShl32(4200000, 8, 8));
    RUN(testSShrShl32(-4200000, 8, 8));
    RUN(testSShrShl32(420000000, 8, 8));
    RUN(testSShrShl32(-420000000, 8, 8));
    
    RUN(testSShrShl64(42, 56, 56));
    RUN(testSShrShl64(-42, 56, 56));
    RUN(testSShrShl64(4200, 56, 56));
    RUN(testSShrShl64(-4200, 56, 56));
    RUN(testSShrShl64(4200000, 56, 56));
    RUN(testSShrShl64(-4200000, 56, 56));
    RUN(testSShrShl64(420000000, 56, 56));
    RUN(testSShrShl64(-420000000, 56, 56));
    RUN(testSShrShl64(42000000000, 56, 56));
    RUN(testSShrShl64(-42000000000, 56, 56));
    
    RUN(testSShrShl64(42, 48, 48));
    RUN(testSShrShl64(-42, 48, 48));
    RUN(testSShrShl64(4200, 48, 48));
    RUN(testSShrShl64(-4200, 48, 48));
    RUN(testSShrShl64(4200000, 48, 48));
    RUN(testSShrShl64(-4200000, 48, 48));
    RUN(testSShrShl64(420000000, 48, 48));
    RUN(testSShrShl64(-420000000, 48, 48));
    RUN(testSShrShl64(42000000000, 48, 48));
    RUN(testSShrShl64(-42000000000, 48, 48));
    
    RUN(testSShrShl64(42, 32, 32));
    RUN(testSShrShl64(-42, 32, 32));
    RUN(testSShrShl64(4200, 32, 32));
    RUN(testSShrShl64(-4200, 32, 32));
    RUN(testSShrShl64(4200000, 32, 32));
    RUN(testSShrShl64(-4200000, 32, 32));
    RUN(testSShrShl64(420000000, 32, 32));
    RUN(testSShrShl64(-420000000, 32, 32));
    RUN(testSShrShl64(42000000000, 32, 32));
    RUN(testSShrShl64(-42000000000, 32, 32));
    
    RUN(testSShrShl64(42, 24, 24));
    RUN(testSShrShl64(-42, 24, 24));
    RUN(testSShrShl64(4200, 24, 24));
    RUN(testSShrShl64(-4200, 24, 24));
    RUN(testSShrShl64(4200000, 24, 24));
    RUN(testSShrShl64(-4200000, 24, 24));
    RUN(testSShrShl64(420000000, 24, 24));
    RUN(testSShrShl64(-420000000, 24, 24));
    RUN(testSShrShl64(42000000000, 24, 24));
    RUN(testSShrShl64(-42000000000, 24, 24));
    
    RUN(testSShrShl64(42, 16, 16));
    RUN(testSShrShl64(-42, 16, 16));
    RUN(testSShrShl64(4200, 16, 16));
    RUN(testSShrShl64(-4200, 16, 16));
    RUN(testSShrShl64(4200000, 16, 16));
    RUN(testSShrShl64(-4200000, 16, 16));
    RUN(testSShrShl64(420000000, 16, 16));
    RUN(testSShrShl64(-420000000, 16, 16));
    RUN(testSShrShl64(42000000000, 16, 16));
    RUN(testSShrShl64(-42000000000, 16, 16));
    
    RUN(testSShrShl64(42, 8, 8));
    RUN(testSShrShl64(-42, 8, 8));
    RUN(testSShrShl64(4200, 8, 8));
    RUN(testSShrShl64(-4200, 8, 8));
    RUN(testSShrShl64(4200000, 8, 8));
    RUN(testSShrShl64(-4200000, 8, 8));
    RUN(testSShrShl64(420000000, 8, 8));
    RUN(testSShrShl64(-420000000, 8, 8));
    RUN(testSShrShl64(42000000000, 8, 8));
    RUN(testSShrShl64(-42000000000, 8, 8));
}

#endif // ENABLE(B3_JIT)
