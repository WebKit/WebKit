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

#include "AllowMacroScratchRegisterUsage.h"
#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3Compilation.h"
#include "B3Const32Value.h"
#include "B3ConstPtrValue.h"
#include "B3ControlValue.h"
#include "B3Effects.h"
#include "B3MemoryValue.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include "CCallHelpers.h"
#include "InitializeThreading.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "PureNaN.h"
#include "VM.h"
#include <cmath>
#include <string>
#include <wtf/Lock.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Threading.h>

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

std::unique_ptr<Compilation> compile(Procedure& procedure, unsigned optLevel = 1)
{
    return std::make_unique<Compilation>(*vm, procedure, optLevel);
}

template<typename T, typename... Arguments>
T invoke(const Compilation& code, Arguments... arguments)
{
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(code.code().executableAddress());
    return function(arguments...);
}

template<typename T, typename... Arguments>
T compileAndRun(Procedure& procedure, Arguments... arguments)
{
    return invoke<T>(*compile(procedure), arguments...);
}

void add32(CCallHelpers& jit, GPRReg src1, GPRReg src2, GPRReg dest)
{
    if (src2 == dest)
        jit.add32(src1, dest);
    else {
        jit.move(src1, dest);
        jit.add32(src2, dest);
    }
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

void testReturnConst64(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), value));

    CHECK(compileAndRun<int64_t>(proc) == value);
}

void testAddArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), value, value));

    CHECK(compileAndRun<int>(proc, a) == a + a);
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

void testAddLoadTwice()
{
    auto test = [&] (unsigned optLevel) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int32_t value = 42;
        Value* load = root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &value));
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Add, Origin(), load, load));

        auto code = compile(proc, optLevel);
        CHECK(invoke<int32_t>(*code) == 42 * 2);
    };

    test(0);
    test(1);
}

void testAddArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a + b));
}

void testMulArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
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
        root->appendNew<ConstPtrValue>(proc, Origin(), &valueSlot));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), mul,
        root->appendNew<ConstPtrValue>(proc, Origin(), &mulSlot));

    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(proc, Mul, Origin(), value, value),
            value));

    CHECK(compileAndRun<int>(proc, a) == a * a + a);
}

void testMulArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Mul, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a * b);
}

void testMulArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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

void testMulLoadTwice()
{
    auto test = [&] (unsigned optLevel) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int32_t value = 42;
        Value* load = root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &value));
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Mul, Origin(), load, load));

        auto code = compile(proc, optLevel);
        CHECK(invoke<int32_t>(*code) == 42 * 42);
    };

    test(0);
    test(1);
}

void testMulArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a * b));
}

void testDivArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a / b));
}

void testSubArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), value, value));

    CHECK(!compileAndRun<int>(proc, a));
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
    root->appendNew<ControlValue>(proc, Return, Origin(), negArgumentMinusOne);
    CHECK(compileAndRun<int>(proc, a) == -a - 1);
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
    root->appendNew<ControlValue>(proc, Return, Origin(), negArgumentMinusOne);
    CHECK(compileAndRun<int>(proc, a) == -a - 1);
}

void testSubArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), valueA, valueB));
    
    CHECK(isIdentical(compileAndRun<double>(proc), a - b));
}

void testBitAndArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a & b));
}

void testBitAndSameArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int64_t>(proc, a) == a);
}

void testBitAndImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a & b));
}

void testBitAndArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a & b));
}

void testBitAndImmArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int64_t>(proc, b) == (a & b));
}

void testBitAndBitAndArgImmImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            innerBitAnd,
            root->appendNew<Const64Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int64_t>(proc, a) == ((a & b) & c));
}

void testBitAndImmBitAndArgImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), c));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            innerBitAnd));

    CHECK(compileAndRun<int64_t>(proc, b) == (a & (b & c)));
}

void testBitAndArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

void testBitAndSameArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int>(proc, a) == a);
}

void testBitAndImms32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc) == (a & b));
}

void testBitAndArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == (a & b));
}

void testBitAndImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == (a & b));
}

void testBitAndBitAndArgImmImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), b));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            innerBitAnd,
            root->appendNew<Const32Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int>(proc, a) == ((a & b) & c));
}

void testBitAndImmBitAndArgImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), c));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            innerBitAnd));

    CHECK(compileAndRun<int>(proc, b) == (a & (b & c)));
}

void testBitAndWithMaskReturnsBooleans(int64_t a, int64_t b)
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

    root->appendNew<ControlValue>(proc, Return, Origin(), select);

    int64_t expected = (a == b) ? -5 : 42;
    CHECK(compileAndRun<int64_t>(proc, a, b) == expected);
}

void testBitOrArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a | b));
}

void testBitOrSameArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int64_t>(proc, a) == a);
}

void testBitOrImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a | b));
}

void testBitOrArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a | b));
}

void testBitOrImmArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int64_t>(proc, b) == (a | b));
}

void testBitOrBitOrArgImmImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            innerBitOr,
            root->appendNew<Const64Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int64_t>(proc, a) == ((a | b) | c));
}

void testBitOrImmBitOrArgImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), c));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            innerBitOr));

    CHECK(compileAndRun<int64_t>(proc, b) == (a | (b | c)));
}

void testBitOrArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

void testBitOrSameArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(
        proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int>(proc, a) == a);
}

void testBitOrImms32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc) == (a | b));
}

void testBitOrArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == (a | b));
}

void testBitOrImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == (a | b));
}

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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            innerBitOr));

    CHECK(compileAndRun<int>(proc, b) == (a | (b | c)));
}

void testBitXorArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            argument,
            argument));

    CHECK(!compileAndRun<int64_t>(proc, a));
}

void testBitXorImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t input = a;
    compileAndRun<int32_t>(proc, &input);
    CHECK(isIdentical(input, static_cast<int32_t>((static_cast<uint32_t>(a) ^ 0xffffffff))));
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
    Value* argsAreNotEqual = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), -1),
        argsAreEqual);

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        argsAreNotEqual,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -42));

    int32_t expectedValue = (a != b) ? 42 : -42;
    CHECK(compileAndRun<int32_t>(proc, a, b) == expectedValue);
}

void testShlArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a << b));
}

void testShlArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a << b));
}

void testShlArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Shl, Origin(), value, value));

    CHECK(compileAndRun<int32_t>(proc, a) == (a << a));
}

void testShlArgs32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc) == (a << b));
}

void testShlArgImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc, a) == (a << b));
}

void testSShrArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a >> b));
}

void testSShrImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a >> b));
}

void testSShrArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a >> b));
}

void testSShrArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), value, value));

    CHECK(compileAndRun<int32_t>(proc, a) == (a >> (a & 31)));
}

void testSShrArgs32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

void testSShrImms32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc) == (a >> b));
}

void testSShrArgImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc, a) == (a >> b));
}

void testZShrArgs(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<uint64_t>(proc, a, b) == (a >> b));
}

void testZShrImms(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint64_t>(proc) == (a >> b));
}

void testZShrArgImm(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint64_t>(proc, a) == (a >> b));
}

void testZShrArg32(uint32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, ZShr, Origin(), value, value));

    CHECK(compileAndRun<uint32_t>(proc, a) == (a >> (a & 31)));
}

void testZShrArgs32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

void testZShrImms32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint32_t>(proc) == (a >> b));
}

void testZShrArgImm32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, a) == countLeadingZero(a));
}

void testClzMem64(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* value = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), value);
    root->appendNew<ControlValue>(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, &a) == countLeadingZero(a));
}

void testClzArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), argument);
    root->appendNew<ControlValue>(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, a) == countLeadingZero(a));
}

void testClzMem32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* value = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), value);
    root->appendNew<ControlValue>(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, &a) == countLeadingZero(a));
}

void testSqrtArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sqrt, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), sqrt(a)));
}

void testSqrtImm(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sqrt, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), sqrt(a)));
}

void testSqrtMem(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sqrt, Origin(), loadDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, &a), sqrt(a)));
}

void testDoubleArgToInt64BitwiseCast(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);

    root->appendNew<ControlValue>(
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

    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(proc, Return, Origin(), second);

    CHECK(isIdentical(compileAndRun<double>(proc, value), value));
}

void testBitwiseCastOnDoubleInMemory(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNew<ControlValue>(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<int64_t>(proc, &value), bitwise_cast<int64_t>(value)));
}

void testInt64BArgToDoubleBitwiseCast(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    root->appendNew<ControlValue>(
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

    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(proc, Return, Origin(), second);

    CHECK(isIdentical(compileAndRun<int64_t>(proc, value), value));
}

void testBitwiseCastOnInt64InMemory(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNew<ControlValue>(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<double>(proc, &value), bitwise_cast<double>(value)));
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

void testNeg32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<intptr_t>(proc, value) == -value);
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
        slotPtr);
    
    root->appendNew<ControlValue>(
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
        slotPtr);
    
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int32_t>(proc));
    CHECK(slot == -value);
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
    
    root->appendNew<ControlValue>(
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

template<typename T>
int32_t modelLoad(int32_t value)
{
    union {
        int32_t original;
        T loaded;
    } u;

    u.original = value;
    if (std::is_signed<T>::value)
        return static_cast<int32_t>(u.loaded);
    return static_cast<int32_t>(static_cast<uint32_t>(u.loaded));
}

template<typename T>
void testLoad(B3::Opcode opcode, int32_t value)
{
    // Simple load from an absolute address.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, Int32, Origin(),
                root->appendNew<ConstPtrValue>(proc, Origin(), &value)));

        CHECK(compileAndRun<int32_t>(proc) == modelLoad<T>(value));
    }
    
    // Simple load from an address in a register.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, Int32, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

        CHECK(compileAndRun<int32_t>(proc, &value) == modelLoad<T>(value));
    }
    
    // Simple load from an address in a register, at an offset.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, Int32, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                sizeof(int32_t)));

        CHECK(compileAndRun<int32_t>(proc, &value - 1) == modelLoad<T>(value));
    }

    // Load from a simple base-index with various scales.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                        root->appendNew<Const32Value>(proc, Origin(), logScale)))));

        CHECK(compileAndRun<int32_t>(proc, &value - 2, (sizeof(int32_t) * 2) >> logScale) == modelLoad<T>(value));
    }

    // Load from a simple base-index with various scales, but commuted.
    for (unsigned logScale = 0; logScale <= 3; ++logScale) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, opcode, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                        root->appendNew<Const32Value>(proc, Origin(), logScale)),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

        CHECK(compileAndRun<int32_t>(proc, &value - 2, (sizeof(int32_t) * 2) >> logScale) == modelLoad<T>(value));
    }
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

    root->appendNew<ControlValue>(proc, Return, Origin(), total);
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

    root->appendNew<ControlValue>(proc, Return, Origin(), total);
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
    root->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(loop));

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
    loop->appendNew<ControlValue>(
        proc, Branch, Origin(),
        decCounter,
        FrequentedBlock(loop), FrequentedBlock(done));

    // Tail.
    done->appendNew<ControlValue>(proc, Return, Origin(), updatedTotal);
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
    root->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(loop));

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
    loop->appendNew<ControlValue>(
        proc, Branch, Origin(),
        decCounter,
        FrequentedBlock(loop), FrequentedBlock(done));

    // Tail.
    done->appendNew<ControlValue>(proc, Return, Origin(), updatedTotal);
    CHECK(isIdentical(compileAndRun<double>(proc, 100000), 5000050000.));
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
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
            unsigned predicateVarIndex = ((i >> 1) + 2) % numVars;
            unsigned thenIncVarIndex = ((i >> 1) + 0) % numVars;
            unsigned elseIncVarIndex = ((i >> 1) + 1) % numVars;

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
            
            Value* startIndex = vars[((i >> 1) + 1) % numVars];
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
            vars[((i >> 1) + 0) % numVars] = finalSum;
        }
    }

    current->appendNew<ControlValue>(proc, Return, Origin(), vars[0]);

    compile(proc);

    double after = monotonicallyIncreasingTimeMS();
    dataLog(toCString("    That took ", after - before, " ms.\n"));
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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), result);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), result);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), result);

    CHECK(compileAndRun<double>(proc, 1.5, 2.5) == 4);
}

void testSimplePatchpointWithOuputClobbersFPArgs()
{
    Procedure proc;
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
    root->appendNew<ControlValue>(proc, Return, Origin(), result);

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
        patchpoint->setGenerator(
            [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                CHECK((params[1].gpr() == GPRInfo::argumentGPR0) == arg1InArgGPR);
                CHECK((params[2].gpr() == GPRInfo::argumentGPR1) == arg2InArgGPR);
                
                add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
            });

        root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc) == (things.size() * (things.size() - 1)) / 2);
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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1) == 43);
}

void testPatchpointManyImms()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), 42);
    Value* arg2 = root->appendNew<Const64Value>(proc, Origin(), 43);
    Value* arg3 = root->appendNew<Const64Value>(proc, Origin(), 43000000000000ll);
    Value* arg4 = root->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg3, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg4, ValueRep::WarmAny));
    patchpoint->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams& params) {
            CHECK(params.size() == 4);
            CHECK(params[0] == ValueRep::constant(42));
            CHECK(params[1] == ValueRep::constant(43));
            CHECK(params[2] == ValueRep::constant(43000000000000ll));
            CHECK(params[3] == ValueRep::constant(bitwise_cast<int64_t>(42.5)));
        });
    root->appendNew<ControlValue>(
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
    patchpoint->resultConstraint = ValueRep::reg(GPRInfo::nonArgGPR0);
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0] == ValueRep::reg(GPRInfo::nonArgGPR0));
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            add32(jit, params[1].gpr(), params[2].gpr(), GPRInfo::nonArgGPR0);
        });
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    patchpoint->resultConstraint = ValueRep::stackArgument(0);
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0] == ValueRep::stack(-static_cast<intptr_t>(proc.frameSize())));
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            jit.store32(params[1].gpr(), CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0));
            jit.add32(params[2].gpr(), CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0));
        });
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

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
    patchpoint->resultConstraint = ValueRep::WarmAny;
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
    root->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<double>(proc, 1, 2) == 3);
}

void testSimpleCheck()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    CheckValue* check = root->appendNew<CheckValue>(proc, Check, Origin(), arg);
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 1);

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    
    CHECK(invoke<int>(*code, 0) == 0);
    CHECK(invoke<int>(*code, 1) == 42);
}

void testCheckLessThan()
{
    Procedure proc;
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
            CHECK(params.size() == 1);

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    
    CHECK(invoke<int>(*code, 42) == 0);
    CHECK(invoke<int>(*code, 1000) == 0);
    CHECK(invoke<int>(*code, 41) == 42);
    CHECK(invoke<int>(*code, 0) == 42);
    CHECK(invoke<int>(*code, -1) == 42);
}

void testCheckMegaCombo()
{
    Procedure proc;
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
            CHECK(params.size() == 1);

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);

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

void testCheckAddImm()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isConstant());
            CHECK(params[3].value() == 42);
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAddImmCommute()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isConstant());
            CHECK(params[3].value() == 42);
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAddImmSomeRegister()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAdd()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0, 42) == 42.0);
    CHECK(invoke<double>(*code, 1, 42) == 43.0);
    CHECK(invoke<double>(*code, 42, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647, 42) == 2147483689.0);
}

void testCheckAdd64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->appendSomeRegister(arg1);
    checkAdd->appendSomeRegister(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt64ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compile(proc);

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
    checkAdd->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            CHECK(!"Should have been folded");
        });
    root->appendNew<ControlValue>(proc, Return, Origin(), checkAdd);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == a + b);
}

void testCheckAddFoldFail(int a, int b)
{
    Procedure proc;
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
    root->appendNew<ControlValue>(proc, Return, Origin(), checkAdd);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckSubImm()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isConstant());
            CHECK(params[3].value() == 42);
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == -42.0);
    CHECK(invoke<double>(*code, 1) == -41.0);
    CHECK(invoke<double>(*code, 42) == 0.0);
    CHECK(invoke<double>(*code, -2147483647) == -2147483689.0);
}

void testCheckSubBadImm()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isConstant());
            CHECK(params[3].value() == badImm);
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(badImm), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == -static_cast<double>(badImm));
    CHECK(invoke<double>(*code, -1) == -static_cast<double>(badImm) - 1);
    CHECK(invoke<double>(*code, 1) == -static_cast<double>(badImm) + 1);
    CHECK(invoke<double>(*code, 42) == -static_cast<double>(badImm) + 42);
}

void testCheckSub()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compile(proc);

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
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->append(arg1);
    checkSub->append(arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt64ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compile(proc);

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
    checkSub->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            CHECK(!"Should have been folded");
        });
    root->appendNew<ControlValue>(proc, Return, Origin(), checkSub);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == a - b);
}

void testCheckSubFoldFail(int a, int b)
{
    Procedure proc;
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
    root->appendNew<ControlValue>(proc, Return, Origin(), checkSub);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckNeg()
{
    Procedure proc;
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
            CHECK(params.size() == 3);
            CHECK(params[2].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT1);
            jit.negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkNeg));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == 0.0);
    CHECK(invoke<double>(*code, 1) == -1.0);
    CHECK(invoke<double>(*code, 42) == -42.0);
    CHECK(invoke<double>(*code, -2147483647 - 1) == 2147483648.0);
}

void testCheckNeg64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const64Value>(proc, Origin(), 0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    CheckValue* checkNeg = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkNeg->append(arg2);
    checkNeg->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[2].isGPR());
            jit.convertInt64ToDouble(params[2].gpr(), FPRInfo::fpRegT1);
            jit.negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkNeg));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0ll) == 0.0);
    CHECK(invoke<double>(*code, 1ll) == -1.0);
    CHECK(invoke<double>(*code, 42ll) == -42.0);
    CHECK(invoke<double>(*code, -9223372036854775807ll - 1) == 9223372036854775808.0);
}

void testCheckMul()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0, 42) == 0.0);
    CHECK(invoke<double>(*code, 1, 42) == 42.0);
    CHECK(invoke<double>(*code, 42, 42) == 42.0 * 42.0);
    CHECK(invoke<double>(*code, 2147483647, 42) == 2147483647.0 * 42.0);
}

void testCheckMulMemory()
{
    Procedure proc;
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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compile(proc);

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
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isConstant());
            CHECK(params[3].value() == 2);
            jit.convertInt32ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(2), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compile(proc);

    CHECK(invoke<double>(*code, 0) == 0.0);
    CHECK(invoke<double>(*code, 1) == 2.0);
    CHECK(invoke<double>(*code, 42) == 42.0 * 2.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483647.0 * 2.0);
}

void testCheckMul64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 4);
            CHECK(params[2].isGPR());
            CHECK(params[3].isGPR());
            jit.convertInt64ToDouble(params[2].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[3].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compile(proc);

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
    checkMul->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            CHECK(!"Should have been folded");
        });
    root->appendNew<ControlValue>(proc, Return, Origin(), checkMul);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == a * b);
}

void testCheckMulFoldFail(int a, int b)
{
    Procedure proc;
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
    root->appendNew<ControlValue>(proc, Return, Origin(), checkMul);

    auto code = compile(proc);

    CHECK(invoke<int>(*code) == 42);
}

template<typename LeftFunctor, typename RightFunctor>
void genericTestCompare(
    B3::Opcode opcode, const LeftFunctor& leftFunctor, const RightFunctor& rightFunctor,
    int left, int right, int result)
{
    // Using a compare.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* leftValue = leftFunctor(root, proc);
        Value* rightValue = rightFunctor(root, proc);
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, NotEqual, Origin(),
                root->appendNew<Value>(proc, opcode, Origin(), leftValue, rightValue),
                root->appendNew<Const32Value>(proc, Origin(), 0)));

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

        root->appendNew<ControlValue>(
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
        thenCase->appendNew<ControlValue>(proc, Return, Origin(), patchpoint);

        elseCase->appendNew<ControlValue>(
            proc, Return, Origin(),
            elseCase->appendNew<Const32Value>(proc, Origin(), 0));

        CHECK(compileAndRun<int>(proc, left, right) == result);
    }
}

int modelCompare(B3::Opcode opcode, int left, int right)
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
        return static_cast<unsigned>(left) > static_cast<unsigned>(right);
    case Below:
        return static_cast<unsigned>(left) < static_cast<unsigned>(right);
    case AboveEqual:
        return static_cast<unsigned>(left) >= static_cast<unsigned>(right);
    case BelowEqual:
        return static_cast<unsigned>(left) <= static_cast<unsigned>(right);
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

void testCompareImpl(B3::Opcode opcode, int left, int right)
{
    int result = modelCompare(opcode, left, right);
    
    // Test tmp-to-tmp.
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
        left, right, result);

    // Test imm-to-tmp.
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
        left, right, result);

    // Test tmp-to-imm.
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
        left, right, result);

    // Test imm-to-imm.
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), right);
        },
        left, right, result);

    testCompareLoad<int32_t>(opcode, Load, left, right);
    testCompareLoad<int8_t>(opcode, Load8S, left, right);
    testCompareLoad<uint8_t>(opcode, Load8Z, left, right);
    testCompareLoad<int16_t>(opcode, Load16S, left, right);
    testCompareLoad<uint16_t>(opcode, Load16Z, left, right);
}

void testCompare(B3::Opcode opcode, int left, int right)
{
    auto variants = [&] (int left, int right) {
        testCompareImpl(opcode, left, right);
        testCompareImpl(opcode, left, right + 1);
        testCompareImpl(opcode, left, right - 1);

        auto multipliedTests = [&] (int factor) {
            testCompareImpl(opcode, left * factor, right);
            testCompareImpl(opcode, left * factor, right + 1);
            testCompareImpl(opcode, left * factor, right - 1);
        
            testCompareImpl(opcode, left, right * factor);
            testCompareImpl(opcode, left, (right + 1) * factor);
            testCompareImpl(opcode, left, (right - 1) * factor);
        
            testCompareImpl(opcode, left * factor, right * factor);
            testCompareImpl(opcode, left * factor, (right + 1) * factor);
            testCompareImpl(opcode, left * factor, (right - 1) * factor);
        };

        multipliedTests(10);
        multipliedTests(100);
        multipliedTests(1000);
        multipliedTests(100000);
    };

    variants(left, right);
    variants(-left, right);
    variants(left, -right);
    variants(-left, -right);
}

int simpleFunction(int a, int b)
{
    return a + b;
}

void testCallSimple(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<void*>(simpleFunction)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

void testCallSimplePure(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Int32, Origin(), Effects::none(),
            root->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<void*>(simpleFunction)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

int functionWithHellaArguments(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p, int q, int r, int s, int t, int u, int v, int w, int x, int y, int z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<void*>(functionWithHellaArguments)));
    call->children().appendVector(args);
    
    root->appendNew<ControlValue>(proc, Return, Origin(), call);

    CHECK(compileAndRun<int>(proc) == functionWithHellaArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

void testReturnDouble(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<ConstDoubleValue>(proc, Origin(), value));

    CHECK(isIdentical(compileAndRun<double>(proc), value));
}

double simpleFunctionDouble(double a, double b)
{
    return a + b;
}

void testCallSimpleDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Double, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<void*>(simpleFunctionDouble)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    CHECK(compileAndRun<double>(proc, a, b) == a + b);
}

double functionWithHellaDoubleArguments(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k, double l, double m, double n, double o, double p, double q, double r, double s, double t, double u, double v, double w, double x, double y, double z)
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
        root->appendNew<ConstPtrValue>(proc, Origin(), bitwise_cast<void*>(functionWithHellaDoubleArguments)));
    call->children().appendVector(args);
    
    root->appendNew<ControlValue>(proc, Return, Origin(), call);

    CHECK(compileAndRun<double>(proc) == functionWithHellaDoubleArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

void testChillDiv(int num, int den, int res)
{
    // Test non-constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
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
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
                root->appendNew<Const32Value>(proc, Origin(), num),
                root->appendNew<Const32Value>(proc, Origin(), den)));
        
        CHECK(compileAndRun<int>(proc) == res);
    }
}

void testChillDivTwice(int num1, int den1, int num2, int den2, int res)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
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
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
        
        CHECK(compileAndRun<int64_t>(proc, num, den) == res);
    }

    // Test constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        
        root->appendNew<ControlValue>(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, ChillDiv, Origin(),
                root->appendNew<Const64Value>(proc, Origin(), num),
                root->appendNew<Const64Value>(proc, Origin(), den)));
        
        CHECK(compileAndRun<int64_t>(proc) == res);
    }
}

void testSwitch(unsigned degree, unsigned gap = 1)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNew<ControlValue>(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 0));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(
        proc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(terminate));

    for (unsigned i = 0; i < degree; ++i) {
        BasicBlock* newBlock = proc.addBlock();
        newBlock->appendNew<ControlValue>(
            proc, Return, Origin(),
            newBlock->appendNew<ArgumentRegValue>(
                proc, Origin(), (i & 1) ? GPRInfo::argumentGPR2 : GPRInfo::argumentGPR1));
        switchValue->appendCase(SwitchCase(gap * i, FrequentedBlock(newBlock)));
    }

    auto code = compile(proc);

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

void testSwitchChillDiv(unsigned degree, unsigned gap = 1)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* left = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* right = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNew<ControlValue>(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 0));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(
        proc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(terminate));

    for (unsigned i = 0; i < degree; ++i) {
        BasicBlock* newBlock = proc.addBlock();

        newBlock->appendNew<ControlValue>(
            proc, Return, Origin(),
            newBlock->appendNew<Value>(
                proc, ChillDiv, Origin(), (i & 1) ? right : left, (i & 1) ? left : right));
        
        switchValue->appendCase(SwitchCase(gap * i, FrequentedBlock(newBlock)));
    }

    auto code = compile(proc);

    for (unsigned i = 0; i < degree; ++i) {
        CHECK(invoke<int32_t>(*code, i * gap, 42, 11) == ((i & 1) ? 11/42 : 42/11));
        if (gap > 1) {
            CHECK(!invoke<int32_t>(*code, i * gap + 1, 42, 11));
            CHECK(!invoke<int32_t>(*code, i * gap - 1, 42, 11));
        }
    }

    CHECK(!invoke<int32_t>(*code, -1, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap + 1, 42, 11));
}

void testTruncFold(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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

void testBasicSelect()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    auto code = compile(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 43, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 43, 642462, 32533) == 32533);
}

void testSelectTest()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    auto code = compile(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 0, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 0, 642462, 32533) == 32533);
}

void testSelectCompareDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    auto code = compile(proc);
    CHECK(invoke<intptr_t>(*code, -1.0, 1.0, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42.5, 42.51, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, PNaN, 0.0, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 42.51, 42.5, 642462, 32533) == 32533);
    CHECK(invoke<intptr_t>(*code, 42.52, 42.52, 524978245, 352) == 352);
}

void testSelectDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    auto code = compile(proc);
    CHECK(invoke<double>(*code, 42, 1.5, 2.6) == 1.5);
    CHECK(invoke<double>(*code, 42, 642462.7, 32533.8) == 642462.7);
    CHECK(invoke<double>(*code, 43, 1.9, 2.0) == 2.0);
    CHECK(invoke<double>(*code, 43, 642462.1, 32533.2) == 32533.2);
}

void testSelectDoubleTest()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    auto code = compile(proc);
    CHECK(invoke<double>(*code, 42, 1.5, 2.6) == 1.5);
    CHECK(invoke<double>(*code, 42, 642462.7, 32533.8) == 642462.7);
    CHECK(invoke<double>(*code, 0, 1.9, 2.0) == 2.0);
    CHECK(invoke<double>(*code, 0, 642462.1, 32533.2) == 32533.2);
}

void testSelectDoubleCompareDouble()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, LessThan, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR2),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR3)));

    auto code = compile(proc);
    CHECK(invoke<double>(*code, -1.0, 1.0, 1.1, 2.2) == 1.1);
    CHECK(invoke<double>(*code, 42.5, 42.51, 642462.3, 32533.4) == 642462.3);
    CHECK(invoke<double>(*code, PNaN, 0.0, 1.5, 2.6) == 2.6);
    CHECK(invoke<double>(*code, 42.51, 42.5, 642462.7, 32533.8) == 32533.8);
    CHECK(invoke<double>(*code, 42.52, 42.52, 524978245.9, 352.0) == 352.0);
}

void testSelectFold(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Select, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<ConstPtrValue>(proc, Origin(), value),
                root->appendNew<ConstPtrValue>(proc, Origin(), 42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    auto code = compile(proc);
    CHECK(invoke<intptr_t>(*code, 1, 2) == (value == 42 ? 1 : 2));
    CHECK(invoke<intptr_t>(*code, 642462, 32533) == (value == 42 ? 642462 : 32533));
}

void testSelectInvert()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

    auto code = compile(proc);
    CHECK(invoke<intptr_t>(*code, 42, 1, 2) == 1);
    CHECK(invoke<intptr_t>(*code, 42, 642462, 32533) == 642462);
    CHECK(invoke<intptr_t>(*code, 43, 1, 2) == 2);
    CHECK(invoke<intptr_t>(*code, 43, 642462, 32533) == 32533);
}

// Make sure the compiler does not try to optimize anything out.
NEVER_INLINE double zero()
{
    return 0.;
}

double negativeZero()
{
    return -zero();
}

// Some convenience functions for brevity of the test output.
double posInfinity()
{
    return std::numeric_limits<double>::infinity();
}

double negInfinity()
{
    return -std::numeric_limits<double>::infinity();
}

template<typename Type>
struct Operand {
    const char* name;
    Type value;
};

typedef Operand<double> DoubleOperand;
typedef Operand<int64_t> Int64Operand;
typedef Operand<int32_t> Int32Operand;

static const std::array<DoubleOperand, 9>& doubleOperands()
{
    static const std::array<DoubleOperand, 9> operands = {{
        { "M_PI", M_PI },
        { "-M_PI", -M_PI },
        { "1.", 1 },
        { "-1.", -1 },
        { "0", 0 },
        { "negativeZero()", negativeZero() },
        { "posInfinity()", posInfinity() },
        { "negInfinity()", negInfinity() },
        { "PNaN", PNaN },
    }};
    return operands;
};

static Vector<Int64Operand> int64Operands()
{
    Vector<Int64Operand> operands;
    for (DoubleOperand doubleOperand : doubleOperands())
        operands.append({ doubleOperand.name, bitwise_cast<int64_t>(doubleOperand.value) });
    operands.append({ "1", 1 });
    operands.append({ "-1", -1 });
    operands.append({ "int64-max", std::numeric_limits<int64_t>::max() });
    operands.append({ "int64-min", std::numeric_limits<int64_t>::min() });
    operands.append({ "int32-max", std::numeric_limits<int32_t>::max() });
    operands.append({ "int32-min", std::numeric_limits<int32_t>::min() });

    return operands;
}

static Vector<Int32Operand> int32Operands()
{
    Vector<Int32Operand> operands({
        { "0", 0 },
        { "1", 1 },
        { "-1", -1 },
        { "42", 42 },
        { "-42", -42 },
        { "int32-max", std::numeric_limits<int32_t>::max() },
        { "int32-min", std::numeric_limits<int32_t>::min() }
    });
    return operands;
}

#define RUN(test) do {                          \
        if (!shouldRun(#test))                  \
            break;                              \
        tasks.append(                           \
            createSharedTask<void()>(           \
                [&] () {                        \
                    dataLog(#test "...\n");     \
                    test;                       \
                    dataLog(#test ": OK!\n");   \
                }));                            \
    } while (false);

#define RUN_UNARY(test, values) \
    for (auto a : values) {                             \
        CString testStr = toCString(#test, "(", a.name, ")"); \
        if (!shouldRun(testStr.data()))                 \
            continue;                                   \
        tasks.append(createSharedTask<void()>(          \
            [=] () {                                    \
                dataLog(testStr, "...\n");              \
                test(a.value);                          \
                dataLog(testStr, ": OK!\n");            \
            }));                                        \
    }

#define RUN_BINARY(test, valuesA, valuesB) \
    for (auto a : valuesA) {                                \
        for (auto b : valuesB) {                            \
            CString testStr = toCString(#test, "(", a.name, ", ", b.name, ")"); \
            if (!shouldRun(testStr.data()))                 \
                continue;                                   \
            tasks.append(createSharedTask<void()>(          \
                [=] () {                                    \
                    dataLog(testStr, "...\n");              \
                    test(a.value, b.value);                 \
                    dataLog(testStr, ": OK!\n");            \
                }));                                        \
        }                                                   \
    }

void run(const char* filter)
{
    JSC::initializeThreading();
    vm = &VM::create(LargeHeap).leakRef();

    Deque<RefPtr<SharedTask<void()>>> tasks;

    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || !!strcasestr(testName, filter);
    };

    RUN(test42());
    RUN(testLoad42());
    RUN(testArg(43));
    RUN(testReturnConst64(5));
    RUN(testReturnConst64(-42));

    RUN(testAddArg(111));
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
    RUN(testAddLoadTwice());

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
    RUN(testMulArgs32(1, 1));
    RUN(testMulArgs32(1, 2));
    RUN(testMulLoadTwice());

    RUN_UNARY(testMulArgDouble, doubleOperands());
    RUN_BINARY(testMulArgsDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testMulArgImmDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testMulImmArgDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testMulImmsDouble, doubleOperands(), doubleOperands());

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
    RUN_UNARY(testNegValueSubOne, int32Operands());

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
    RUN_UNARY(testNegValueSubOne32, int64Operands());

    RUN_UNARY(testSubArgDouble, doubleOperands());
    RUN_BINARY(testSubArgsDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testSubArgImmDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testSubImmArgDouble, doubleOperands(), doubleOperands());
    RUN_BINARY(testSubImmsDouble, doubleOperands(), doubleOperands());

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
    RUN(testBitAndBitAndArgImmImm32(2, 7, 3));
    RUN(testBitAndBitAndArgImmImm32(1, 6, 6));
    RUN(testBitAndBitAndArgImmImm32(0xffff, 24, 7));
    RUN(testBitAndImmBitAndArgImm32(7, 2, 3));
    RUN(testBitAndImmBitAndArgImm32(6, 1, 6));
    RUN(testBitAndImmBitAndArgImm32(24, 0xffff, 7));
    RUN_BINARY(testBitAndWithMaskReturnsBooleans, int64Operands(), int64Operands());

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

    RUN_UNARY(testBitNotArg, int64Operands());
    RUN_UNARY(testBitNotImm, int64Operands());
    RUN_UNARY(testBitNotMem, int64Operands());
    RUN_UNARY(testBitNotArg32, int32Operands());
    RUN_UNARY(testBitNotImm32, int32Operands());
    RUN_UNARY(testBitNotMem32, int32Operands());
    RUN_BINARY(testBitNotOnBooleanAndBranch32, int32Operands(), int32Operands());

    RUN(testShlArgs(1, 0));
    RUN(testShlArgs(1, 1));
    RUN(testShlArgs(1, 62));
    RUN(testShlArgs(0xffffffffffffffff, 0));
    RUN(testShlArgs(0xffffffffffffffff, 1));
    RUN(testShlArgs(0xffffffffffffffff, 63));
    RUN(testShlImms(1, 0));
    RUN(testShlImms(1, 1));
    RUN(testShlImms(1, 62));
    RUN(testShlImms(1, 65));
    RUN(testShlImms(0xffffffffffffffff, 0));
    RUN(testShlImms(0xffffffffffffffff, 1));
    RUN(testShlImms(0xffffffffffffffff, 63));
    RUN(testShlArgImm(1, 0));
    RUN(testShlArgImm(1, 1));
    RUN(testShlArgImm(1, 62));
    RUN(testShlArgImm(1, 65));
    RUN(testShlArgImm(0xffffffffffffffff, 0));
    RUN(testShlArgImm(0xffffffffffffffff, 1));
    RUN(testShlArgImm(0xffffffffffffffff, 63));
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
    RUN(testShlArgImm32(0xffffffff, 0));
    RUN(testShlArgImm32(0xffffffff, 1));
    RUN(testShlArgImm32(0xffffffff, 63));

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

    RUN_UNARY(testClzArg64, int64Operands());
    RUN_UNARY(testClzMem64, int64Operands());
    RUN_UNARY(testClzArg32, int32Operands());
    RUN_UNARY(testClzMem32, int64Operands());

    RUN_UNARY(testSqrtArg, doubleOperands());
    RUN_UNARY(testSqrtImm, doubleOperands());
    RUN_UNARY(testSqrtMem, doubleOperands());

    RUN_UNARY(testDoubleArgToInt64BitwiseCast, doubleOperands());
    RUN_UNARY(testDoubleImmToInt64BitwiseCast, doubleOperands());
    RUN_UNARY(testTwoBitwiseCastOnDouble, doubleOperands());
    RUN_UNARY(testBitwiseCastOnDoubleInMemory, doubleOperands());
    RUN_UNARY(testInt64BArgToDoubleBitwiseCast, int64Operands());
    RUN_UNARY(testInt64BImmToDoubleBitwiseCast, int64Operands());
    RUN_UNARY(testTwoBitwiseCastOnInt64, int64Operands());
    RUN_UNARY(testBitwiseCastOnInt64InMemory, int64Operands());

    RUN(testStore(44));
    RUN(testStoreConstant(49));
    RUN(testStoreConstantPtr(49));
    RUN(testTrunc((static_cast<int64_t>(1) << 40) + 42));
    RUN(testAdd1(45));
    RUN(testAdd1Ptr(51));
    RUN(testAdd1Ptr(bitwise_cast<intptr_t>(vm)));
    RUN(testNeg32(52));
    RUN(testNegPtr(53));
    RUN(testStoreAddLoad(46));
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

    RUN(testSimplePatchpoint());
    RUN(testSimplePatchpointWithoutOuputClobbersGPArgs());
    RUN(testSimplePatchpointWithOuputClobbersGPArgs());
    RUN(testSimplePatchpointWithoutOuputClobbersFPArgs());
    RUN(testSimplePatchpointWithOuputClobbersFPArgs());
    RUN(testPatchpointWithEarlyClobber());
    RUN(testPatchpointCallArg());
    RUN(testPatchpointFixedRegister());
    RUN(testPatchpointAny(ValueRep::WarmAny));
    RUN(testPatchpointAny(ValueRep::ColdAny));
    RUN(testPatchpointLotsOfLateAnys());
    RUN(testPatchpointAnyImm(ValueRep::WarmAny));
    RUN(testPatchpointAnyImm(ValueRep::ColdAny));
    RUN(testPatchpointAnyImm(ValueRep::LateColdAny));
    RUN(testPatchpointManyImms());
    RUN(testPatchpointWithRegisterResult());
    RUN(testPatchpointWithStackArgumentResult());
    RUN(testPatchpointWithAnyResult());
    RUN(testSimpleCheck());
    RUN(testCheckLessThan());
    RUN(testCheckMegaCombo());
    RUN(testCheckAddImm());
    RUN(testCheckAddImmCommute());
    RUN(testCheckAddImmSomeRegister());
    RUN(testCheckAdd());
    RUN(testCheckAdd64());
    RUN(testCheckAddFold(100, 200));
    RUN(testCheckAddFoldFail(2147483647, 100));
    RUN(testCheckSubImm());
    RUN(testCheckSubBadImm());
    RUN(testCheckSub());
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

    RUN(testCompare(Equal, 42, 42));
    RUN(testCompare(NotEqual, 42, 42));
    RUN(testCompare(LessThan, 42, 42));
    RUN(testCompare(GreaterThan, 42, 42));
    RUN(testCompare(LessEqual, 42, 42));
    RUN(testCompare(GreaterEqual, 42, 42));
    RUN(testCompare(Below, 42, 42));
    RUN(testCompare(Above, 42, 42));
    RUN(testCompare(BelowEqual, 42, 42));
    RUN(testCompare(AboveEqual, 42, 42));

    RUN(testCompare(BitAnd, 42, 42));
    RUN(testCompare(BitAnd, 42, 0));

    RUN(testLoad<int32_t>(Load, 60));
    RUN(testLoad<int32_t>(Load, -60));
    RUN(testLoad<int32_t>(Load, 1000));
    RUN(testLoad<int32_t>(Load, -1000));
    RUN(testLoad<int32_t>(Load, 1000000));
    RUN(testLoad<int32_t>(Load, -1000000));
    RUN(testLoad<int32_t>(Load, 1000000000));
    RUN(testLoad<int32_t>(Load, -1000000000));
    
    RUN(testLoad<int8_t>(Load8S, 60));
    RUN(testLoad<int8_t>(Load8S, -60));
    RUN(testLoad<int8_t>(Load8S, 1000));
    RUN(testLoad<int8_t>(Load8S, -1000));
    RUN(testLoad<int8_t>(Load8S, 1000000));
    RUN(testLoad<int8_t>(Load8S, -1000000));
    RUN(testLoad<int8_t>(Load8S, 1000000000));
    RUN(testLoad<int8_t>(Load8S, -1000000000));
    
    RUN(testLoad<uint8_t>(Load8Z, 60));
    RUN(testLoad<uint8_t>(Load8Z, -60));
    RUN(testLoad<uint8_t>(Load8Z, 1000));
    RUN(testLoad<uint8_t>(Load8Z, -1000));
    RUN(testLoad<uint8_t>(Load8Z, 1000000));
    RUN(testLoad<uint8_t>(Load8Z, -1000000));
    RUN(testLoad<uint8_t>(Load8Z, 1000000000));
    RUN(testLoad<uint8_t>(Load8Z, -1000000000));

    RUN(testLoad<int16_t>(Load16S, 60));
    RUN(testLoad<int16_t>(Load16S, -60));
    RUN(testLoad<int16_t>(Load16S, 1000));
    RUN(testLoad<int16_t>(Load16S, -1000));
    RUN(testLoad<int16_t>(Load16S, 1000000));
    RUN(testLoad<int16_t>(Load16S, -1000000));
    RUN(testLoad<int16_t>(Load16S, 1000000000));
    RUN(testLoad<int16_t>(Load16S, -1000000000));
    
    RUN(testLoad<uint16_t>(Load16Z, 60));
    RUN(testLoad<uint16_t>(Load16Z, -60));
    RUN(testLoad<uint16_t>(Load16Z, 1000));
    RUN(testLoad<uint16_t>(Load16Z, -1000));
    RUN(testLoad<uint16_t>(Load16Z, 1000000));
    RUN(testLoad<uint16_t>(Load16Z, -1000000));
    RUN(testLoad<uint16_t>(Load16Z, 1000000000));
    RUN(testLoad<uint16_t>(Load16Z, -1000000000));

    RUN(testSpillGP());
    RUN(testSpillFP());

    RUN(testInt32ToDoublePartialRegisterStall());
    RUN(testInt32ToDoublePartialRegisterWithoutStall());

    RUN(testCallSimple(1, 2));
    RUN(testCallSimplePure(1, 2));
    RUN(testCallFunctionWithHellaArguments());

    RUN(testReturnDouble(0.0));
    RUN(testReturnDouble(negativeZero()));
    RUN(testReturnDouble(42.5));

    RUN(testCallSimpleDouble(1, 2));
    RUN(testCallFunctionWithHellaDoubleArguments());

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

    RUN(testSwitch(0, 1));
    RUN(testSwitch(1, 1));
    RUN(testSwitch(2, 1));
    RUN(testSwitch(2, 2));
    RUN(testSwitch(10, 1));
    RUN(testSwitch(10, 2));
    RUN(testSwitch(100, 1));
    RUN(testSwitch(100, 100));

    RUN(testSwitchChillDiv(0, 1));
    RUN(testSwitchChillDiv(1, 1));
    RUN(testSwitchChillDiv(2, 1));
    RUN(testSwitchChillDiv(2, 2));
    RUN(testSwitchChillDiv(10, 1));
    RUN(testSwitchChillDiv(10, 2));
    RUN(testSwitchChillDiv(100, 1));
    RUN(testSwitchChillDiv(100, 100));

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

    RUN(testBasicSelect());
    RUN(testSelectTest());
    RUN(testSelectCompareDouble());
    RUN(testSelectDouble());
    RUN(testSelectDoubleTest());
    RUN(testSelectDoubleCompareDouble());
    RUN(testSelectFold(42));
    RUN(testSelectFold(43));
    RUN(testSelectInvert());

    if (tasks.isEmpty())
        usage();

    Lock lock;

    Vector<ThreadIdentifier> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(
            createThread(
                "testb3 thread",
                [&] () {
                    for (;;) {
                        RefPtr<SharedTask<void()>> task;
                        {
                            LockHolder locker(lock);
                            if (tasks.isEmpty())
                                return;
                            task = tasks.takeFirst();
                        }

                        task->run();
                    }
                }));
    }

    for (ThreadIdentifier thread : threads)
        waitForThreadCompletion(thread);
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
    const char* filter = nullptr;
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

