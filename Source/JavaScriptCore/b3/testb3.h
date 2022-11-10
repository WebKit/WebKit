/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirStackSlot.h"
#include "AirValidate.h"
#include "AllowMacroScratchRegisterUsage.h"
#include "B3ArgumentRegValue.h"
#include "B3AtomicValue.h"
#include "B3BasicBlockInlines.h"
#include "B3BreakCriticalEdges.h"
#include "B3CCallValue.h"
#include "B3Compile.h"
#include "B3ComputeDivisionMagic.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3ConstPtrValue.h"
#include "B3Effects.h"
#include "B3FenceValue.h"
#include "B3FixSSA.h"
#include "B3Generate.h"
#include "B3LowerToAir.h"
#include "B3MathExtras.h"
#include "B3MemoryValue.h"
#include "B3MoveConstants.h"
#include "B3NativeTraits.h"
#include "B3Procedure.h"
#include "B3ReduceStrength.h"
#include "B3SlotBaseValue.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3UseCounts.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include "CCallHelpers.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "InitializeThreading.h"
#include "JITCompilation.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "PureNaN.h"
#include <cmath>
#include <regex>
#include <string>
#include <wtf/FastTLS.h>
#include <wtf/IndexSet.h>
#include <wtf/ListDump.h>
#include <wtf/Lock.h>
#include <wtf/NumberOfCores.h>
#include <wtf/StdList.h>
#include <wtf/Threading.h>
#include <wtf/text/StringCommon.h>

// We don't have a NO_RETURN_DUE_TO_EXIT, nor should we. That's ridiculous.
inline bool hiddenTruthBecauseNoReturnIsStupid() { return true; }

inline void usage()
{
    dataLog("Usage: testb3 [<filter>]\n");
    if (hiddenTruthBecauseNoReturnIsStupid())
        exit(1);
}

#if ENABLE(B3_JIT)

using namespace JSC;
using namespace JSC::B3;

inline bool shouldBeVerbose(Procedure& procedure)
{
    return shouldDumpIR(procedure, B3Mode);
}

extern Lock crashLock;

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK(x) do {                                                   \
    if (!!(x))                                                      \
        break;                                                      \
    crashLock.lock();                                               \
    WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #x); \
    CRASH();                                                        \
} while (false)
    
#define CHECK_EQ(x, y) do { \
    auto __x = (x); \
    auto __y = (y); \
    if (__x == __y) \
        break; \
    crashLock.lock(); \
    WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, toCString(#x " == " #y, " (" #x " == ", __x, ", " #y " == ", __y, ")").data()); \
    CRASH(); \
} while (false)

#define PREFIX "O", Options::defaultB3OptLevel(), ": "

#define RUN(test) do {                             \
    if (!shouldRun(filter, #test))                 \
        break;                                     \
    tasks.append(                                  \
        createSharedTask<void()>(                  \
            [&] () {                               \
                dataLog(PREFIX #test "...\n");     \
                test;                              \
                dataLog(PREFIX #test ": OK!\n");   \
            }));                                   \
    } while (false);

#define RUN_UNARY(test, values) \
    for (auto a : values) {                             \
        CString testStr = toCString(PREFIX #test, "(", a.name, ")"); \
        if (!shouldRun(filter, testStr.data()))         \
            continue;                                   \
        tasks.append(createSharedTask<void()>(          \
            [=] () {                                    \
                dataLog(toCString(testStr, "...\n"));   \
                test(a.value);                          \
                dataLog(toCString(testStr, ": OK!\n")); \
            }));                                        \
    }

#define RUN_NOW(test) do {                      \
        if (!shouldRun(filter, #test))          \
            break;                              \
        dataLog(PREFIX #test "...\n");          \
        test;                                   \
        dataLog(PREFIX #test ": OK!\n");        \
    } while (false)

#define RUN_BINARY(test, valuesA, valuesB) \
    for (auto a : valuesA) {                                \
        for (auto b : valuesB) {                            \
            CString testStr = toCString(PREFIX #test, "(", a.name, ", ", b.name, ")"); \
            if (!shouldRun(filter, testStr.data()))         \
                continue;                                   \
            tasks.append(createSharedTask<void()>(          \
                [=] () {                                    \
                    dataLog(toCString(testStr, "...\n"));   \
                    test(a.value, b.value);                 \
                    dataLog(toCString(testStr, ": OK!\n")); \
                }));                                        \
        }                                                   \
    }
#define RUN_TERNARY(test, valuesA, valuesB, valuesC) \
    for (auto a : valuesA) {                                    \
        for (auto b : valuesB) {                                \
            for (auto c : valuesC) {                            \
                CString testStr = toCString(#test, "(", a.name, ", ", b.name, ",", c.name, ")"); \
                if (!shouldRun(filter, testStr.data()))         \
                    continue;                                   \
                tasks.append(createSharedTask<void()>(          \
                    [=] () {                                    \
                        dataLog(toCString(testStr, "...\n"));   \
                        test(a.value, b.value, c.value);        \
                        dataLog(toCString(testStr, ": OK!\n")); \
                    }));                                        \
            }                                                   \
        }                                                       \
    }


inline std::unique_ptr<Compilation> compileProc(Procedure& procedure, unsigned optLevel = Options::defaultB3OptLevel())
{
    procedure.setOptLevel(optLevel);
    return makeUnique<Compilation>(B3::compile(procedure));
}

template<typename T, typename... Arguments>
T invoke(CodePtr<JITCompilationPtrTag> ptr, Arguments... arguments)
{
    void* executableAddress = untagCFunctionPtr<JITCompilationPtrTag>(ptr.taggedPtr());
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(executableAddress);
    return function(arguments...);
}

template<typename T, typename... Arguments>
T invoke(const Compilation& code, Arguments... arguments)
{
    return invoke<T>(code.code(), arguments...);
}

template<typename T, typename... Arguments>
T compileAndRun(Procedure& procedure, Arguments... arguments)
{
    return invoke<T>(*compileProc(procedure), arguments...);
}

inline void lowerToAirForTesting(Procedure& proc)
{
    proc.resetReachability();
    
    if (shouldBeVerbose(proc))
        dataLog("B3 before lowering:\n", proc);
    
    validate(proc);
    lowerToAir(proc);
    
    if (shouldBeVerbose(proc))
        dataLog("Air after lowering:\n", proc.code());
    
    Air::validate(proc.code());
}

template<typename Func>
void checkDisassembly(Compilation& compilation, const Func& func, const CString& failText)
{
    CString disassembly = compilation.disassembly();
    if (func(disassembly.data()))
        return;
    
    crashLock.lock();
    dataLog("Bad lowering!  ", failText, "\n");
    dataLog(disassembly);
    CRASH();
}

inline void checkUsesInstruction(Compilation& compilation, const char* text, bool regex = false)
{
    checkDisassembly(
        compilation,
        [&] (const char* disassembly) -> bool {
            if (regex)
                return std::regex_match(disassembly, std::regex(text, std::regex::extended));
            return strstr(disassembly, text);
        },
        toCString("Expected to find ", text, " but didnt!"));
}

inline void checkDoesNotUseInstruction(Compilation& compilation, const char* text)
{
    checkDisassembly(
        compilation,
        [&] (const char* disassembly) -> bool {
            return !strstr(disassembly, text);
        },
        toCString("Did not expected to find ", text, " but it's there!"));
}

template<typename Type>
struct B3Operand {
    const char* name;
    Type value;
};

typedef B3Operand<int64_t> Int64Operand;
typedef B3Operand<int32_t> Int32Operand;
typedef B3Operand<int16_t> Int16Operand;
typedef B3Operand<int8_t> Int8Operand;

#define MAKE_OPERAND(value) B3Operand<decltype(value)> { #value, value }

template<typename FloatType>
void populateWithInterestingValues(Vector<B3Operand<FloatType>>& operands)
{
    operands.append({ "0.", static_cast<FloatType>(0.) });
    operands.append({ "-0.", static_cast<FloatType>(-0.) });
    operands.append({ "0.4", static_cast<FloatType>(0.5) });
    operands.append({ "-0.4", static_cast<FloatType>(-0.5) });
    operands.append({ "0.5", static_cast<FloatType>(0.5) });
    operands.append({ "-0.5", static_cast<FloatType>(-0.5) });
    operands.append({ "0.6", static_cast<FloatType>(0.6) });
    operands.append({ "-0.6", static_cast<FloatType>(-0.6) });
    operands.append({ "1.", static_cast<FloatType>(1.) });
    operands.append({ "-1.", static_cast<FloatType>(-1.) });
    operands.append({ "1.1", static_cast<FloatType>(1.1) });
    operands.append({ "-1.1", static_cast<FloatType>(-1.1) });
    operands.append({ "2.", static_cast<FloatType>(2.) });
    operands.append({ "-2.", static_cast<FloatType>(-2.) });
    operands.append({ "M_PI", static_cast<FloatType>(M_PI) });
    operands.append({ "-M_PI", static_cast<FloatType>(-M_PI) });
    operands.append({ "min", std::numeric_limits<FloatType>::min() });
    operands.append({ "max", std::numeric_limits<FloatType>::max() });
    operands.append({ "lowest", std::numeric_limits<FloatType>::lowest() });
    operands.append({ "epsilon", std::numeric_limits<FloatType>::epsilon() });
    operands.append({ "infiniti", std::numeric_limits<FloatType>::infinity() });
    operands.append({ "-infiniti", - std::numeric_limits<FloatType>::infinity() });
    operands.append({ "PNaN", static_cast<FloatType>(PNaN) });
}

template<typename FloatType>
Vector<B3Operand<FloatType>> floatingPointOperands()
{
    Vector<B3Operand<FloatType>> operands;
    populateWithInterestingValues(operands);
    return operands;
};

inline Vector<Int64Operand> int64Operands()
{
    Vector<Int64Operand> operands;
    operands.append({ "0", 0 });
    operands.append({ "1", 1 });
    operands.append({ "-1", -1 });
    operands.append({ "42", 42 });
    operands.append({ "-42", -42 });
    operands.append({ "int64-max", std::numeric_limits<int64_t>::max() });
    operands.append({ "int64-min", std::numeric_limits<int64_t>::min() });
    operands.append({ "int32-max", std::numeric_limits<int32_t>::max() });
    operands.append({ "int32-min", std::numeric_limits<int32_t>::min() });
    operands.append({ "uint64-max", static_cast<int64_t>(std::numeric_limits<uint64_t>::max()) });
    operands.append({ "uint64-min", static_cast<int64_t>(std::numeric_limits<uint64_t>::min()) });
    operands.append({ "uint32-max", static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) });
    operands.append({ "uint32-min", static_cast<int64_t>(std::numeric_limits<uint32_t>::min()) });
    
    return operands;
}

inline Vector<Int32Operand> int32Operands()
{
    Vector<Int32Operand> operands({
        { "0", 0 },
        { "1", 1 },
        { "-1", -1 },
        { "42", 42 },
        { "-42", -42 },
        { "int32-max", std::numeric_limits<int32_t>::max() },
        { "int32-min", std::numeric_limits<int32_t>::min() },
        { "uint32-max", static_cast<int32_t>(std::numeric_limits<uint32_t>::max()) },
        { "uint32-min", static_cast<int32_t>(std::numeric_limits<uint32_t>::min()) }
    });
    return operands;
}

inline Vector<Int16Operand> int16Operands()
{
    Vector<Int16Operand> operands({
        { "0", 0 },
        { "1", 1 },
        { "-1", -1 },
        { "42", 42 },
        { "-42", -42 },
        { "int16-max", std::numeric_limits<int16_t>::max() },
        { "int16-min", std::numeric_limits<int16_t>::min() },
        { "uint16-max", static_cast<int16_t>(std::numeric_limits<uint16_t>::max()) },
        { "uint16-min", static_cast<int16_t>(std::numeric_limits<uint16_t>::min()) }
    });
    return operands;
}

inline Vector<Int8Operand> int8Operands()
{
    Vector<Int8Operand> operands({
        { "0", 0 },
        { "1", 1 },
        { "-1", -1 },
        { "42", 42 },
        { "-42", -42 },
        { "int8-max", std::numeric_limits<int8_t>::max() },
        { "int8-min", std::numeric_limits<int8_t>::min() },
        { "uint8-max", static_cast<int8_t>(std::numeric_limits<uint8_t>::max()) },
        { "uint8-min", static_cast<int8_t>(std::numeric_limits<uint8_t>::min()) }
    });
    return operands;
}

inline void add32(CCallHelpers& jit, GPRReg src1, GPRReg src2, GPRReg dest)
{
    if (src2 == dest)
        jit.add32(src1, dest);
    else {
        jit.move(src1, dest);
        jit.add32(src2, dest);
    }
}

template<typename LoadedType, typename EffectiveType>
EffectiveType modelLoad(EffectiveType value);

template<typename LoadedType, typename EffectiveType>
EffectiveType modelLoad(EffectiveType value)
{
    union {
        EffectiveType original;
        LoadedType loaded;
    } u;
    
    u.original = value;
    if (std::is_signed<LoadedType>::value)
        return static_cast<EffectiveType>(u.loaded);
    return static_cast<EffectiveType>(static_cast<typename std::make_unsigned<EffectiveType>::type>(u.loaded));
}

template<>
inline float modelLoad<float, float>(float value) { return value; }

template<>
inline double modelLoad<double, double>(double value) { return value; }

void run(const char* filter);
void testBitAndSExt32(int32_t value, int64_t mask);
void testUbfx32ShiftAnd();
void testUbfx32AndShift();
void testUbfx64ShiftAnd();
void testUbfx64AndShift();
void testUbfiz32AndShiftValueMask();
void testUbfiz32AndShiftMaskValue();
void testUbfiz32ShiftAnd();
void testUbfiz32AndShift();
void testUbfiz64AndShiftValueMask();
void testUbfiz64AndShiftMaskValue();
void testUbfiz64ShiftAnd();
void testUbfiz64AndShift();
void testInsertBitField32();
void testInsertBitField64();
void testExtractInsertBitfieldAtLowEnd32();
void testExtractInsertBitfieldAtLowEnd64();
void testBIC32();
void testBIC64();
void testOrNot32();
void testOrNot64();
void testXorNot32();
void testXorNot64();
void testXorNotWithLeftShift32();
void testXorNotWithRightShift32();
void testXorNotWithUnsignedRightShift32();
void testXorNotWithLeftShift64();
void testXorNotWithRightShift64();
void testXorNotWithUnsignedRightShift64();
void testBitfieldZeroExtend32();
void testBitfieldZeroExtend64();
void testExtractRegister32();
void testExtractRegister64();
void testInsertSignedBitfieldInZero32();
void testInsertSignedBitfieldInZero64();
void testExtractSignedBitfield32();
void testExtractSignedBitfield64();
void testBitAndZeroShiftRightArgImmMask32();
void testBitAndZeroShiftRightArgImmMask64();
void testBasicSelect();
void testSelectTest();
void testAddWithLeftShift32();
void testAddWithRightShift32();
void testAddWithUnsignedRightShift32();
void testAddWithLeftShift64();
void testAddWithRightShift64();
void testAddWithUnsignedRightShift64();
void testSubWithLeftShift32();
void testSubWithRightShift32();
void testSubWithUnsignedRightShift32();
void testSubWithLeftShift64();
void testSubWithRightShift64();
void testSubWithUnsignedRightShift64();

void testAndLeftShift32();
void testAndRightShift32();
void testAndUnsignedRightShift32();
void testAndLeftShift64();
void testAndRightShift64();
void testAndUnsignedRightShift64();

void testXorLeftShift32();
void testXorRightShift32();
void testXorUnsignedRightShift32();
void testXorLeftShift64();
void testXorRightShift64();
void testXorUnsignedRightShift64();

void testOrLeftShift32();
void testOrRightShift32();
void testOrUnsignedRightShift32();
void testOrLeftShift64();
void testOrRightShift64();
void testOrUnsignedRightShift64();

void testSelectCompareDouble();
void testSelectCompareFloat(float, float);
void testSelectCompareFloatToDouble(float, float);
void testSelectDouble();
void testSelectDoubleTest();
void testSelectDoubleCompareDouble();
void testSelectDoubleCompareFloat(float, float);
void testSelectFloatCompareFloat(float, float);
void testSelectDoubleCompareDoubleWithAliasing();
void testSelectFloatCompareFloatWithAliasing();
void testSelectFold(intptr_t value);
void testSelectInvert();
void testCheckSelect();
void testCheckSelectCheckSelect();
void testCheckSelectAndCSE();
void testPowDoubleByIntegerLoop(double xOperand, int32_t yOperand);
double b3Pow(double x, int y);
void testTruncOrHigh();
void testTruncOrLow();
void testBitAndOrHigh();
void testBitAndOrLow();
void testBranch64Equal(int64_t left, int64_t right);
void testBranch64EqualImm(int64_t left, int64_t right);
void testBranch64EqualMem(int64_t left, int64_t right);
void testBranch64EqualMemImm(int64_t left, int64_t right);
void testStore8Load8Z(int32_t value);
void testStore16Load16Z(int32_t value);
void testTrivialInfiniteLoop();
void testFoldPathEqual();
void testLShiftSelf32();
void testRShiftSelf32();
void testURShiftSelf32();
void testLShiftSelf64();
void testRShiftSelf64();
void testURShiftSelf64();
void testPatchpointDoubleRegs();
void testSpillDefSmallerThanUse();
void testSpillUseLargerThanDef();
void testLateRegister();
extern "C" void interpreterPrint(Vector<intptr_t>* stream, intptr_t value);
void testInterpreter();
void testReduceStrengthCheckBottomUseInAnotherBlock();
void testResetReachabilityDanglingReference();
void testEntrySwitchSimple();
void testEntrySwitchNoEntrySwitch();
void testEntrySwitchWithCommonPaths();
void testEntrySwitchWithCommonPathsAndNonTrivialEntrypoint();
void testEntrySwitchLoop();
void testSomeEarlyRegister();
void testBranchBitAndImmFusion(B3::Opcode valueModifier, Type valueType, int64_t constant, Air::Opcode expectedOpcode, Air::Arg::Kind firstKind);
void testTerminalPatchpointThatNeedsToBeSpilled();
void testTerminalPatchpointThatNeedsToBeSpilled2();
void testPatchpointTerminalReturnValue(bool successIsRare);
void testMemoryFence();
void testStoreFence();
void testLoadFence();
void testTrappingLoad();
void testTrappingStore();
void testTrappingLoadAddStore();
void testTrappingLoadDCE();
void testTrappingStoreElimination();
void testMoveConstants();
void testMoveConstantsWithLargeOffsets();
void testPCOriginMapDoesntInsertNops();
void testBitOrBitOrArgImmImm32(int, int, int c);
void testBitOrImmBitOrArgImm32(int, int, int c);
double bitOrDouble(double, double);
void testBitOrArgDouble(double);
void testBitOrArgsDouble(double, double);
void testBitOrArgImmDouble(double, double);
void testBitOrImmsDouble(double, double);
float bitOrFloat(float, float);
void testBitOrArgFloat(float);
void testBitOrArgsFloat(float, float);
void testBitOrArgImmFloat(float, float);
void testBitOrImmsFloat(float, float);
void testBitOrArgsFloatWithUselessDoubleConversion(float, float);
void testBitXorArgs(int64_t, int64_t);
void testBitXorSameArg(int64_t);
void testBitXorAndAndArgs(int64_t, int64_t, int64_t c);
void testBitXorAndAndArgs32(int32_t, int32_t, int32_t c);
void testBitXorAndSameArgs(int64_t, int64_t);
void testBitXorAndSameArgs32(int32_t, int32_t);
void testBitXorImms(int64_t, int64_t);
void testBitXorArgImm(int64_t, int64_t);
void testBitXorImmArg(int64_t, int64_t);
void testBitXorBitXorArgImmImm(int64_t, int64_t, int64_t c);
void testBitXorImmBitXorArgImm(int64_t, int64_t, int64_t c);
void testBitXorArgs32(int, int);
void testBitXorSameArg32(int);
void testBitXorImms32(int, int);
void testBitXorArgImm32(int, int);
void testBitXorImmArg32(int, int);
void testBitXorBitXorArgImmImm32(int, int, int c);
void testBitXorImmBitXorArgImm32(int, int, int c);
void testBitNotArg(int64_t);
void testBitNotImm(int64_t);
void testBitNotMem(int64_t);
void testBitNotArg32(int32_t);
void testBitNotImm32(int32_t);
void testBitNotMem32(int32_t);
void testNotOnBooleanAndBranch32(int64_t, int64_t);
void testBitNotOnBooleanAndBranch32(int64_t, int64_t);
void testShlArgs(int64_t, int64_t);
void testShlImms(int64_t, int64_t);
void testShlArgImm(int64_t, int64_t);
void testShlSShrArgImm(int64_t, int64_t);
void testShlArg32(int32_t);
void testShlArgs32(int32_t, int32_t);
void testShlImms32(int32_t, int32_t);
void testShlArgImm32(int32_t, int32_t);
void testShlZShrArgImm32(int32_t, int32_t);
void testClzArg64(int64_t);
void testClzMem64(int64_t);
void testClzArg32(int32_t);
void testClzMem32(int32_t);
void testAbsArg(double);
void testAbsImm(double);
void testAbsMem(double);
void testAbsAbsArg(double);
void testAbsNegArg(double);
void testAbsBitwiseCastArg(double);
void testBitwiseCastAbsBitwiseCastArg(double);
void testAbsArg(float);
void testAbsImm(float);
void testAbsMem(float);
void testAbsAbsArg(float);
void testAbsNegArg(float);
void testAbsBitwiseCastArg(float);
void testBitwiseCastAbsBitwiseCastArg(float);
void testAbsArgWithUselessDoubleConversion(float);
void testAbsArgWithEffectfulDoubleConversion(float);
void testCeilArg(double);
void testCeilImm(double);
void testCeilMem(double);
void testCeilCeilArg(double);
void testFloorCeilArg(double);
void testCeilIToD64(int64_t);
void testCeilIToD32(int64_t);
void testCeilArg(float);
void testCeilImm(float);
void testCeilMem(float);
void testCeilCeilArg(float);
void testFloorCeilArg(float);
void testCeilArgWithUselessDoubleConversion(float);
void testCeilArgWithEffectfulDoubleConversion(float);
void testFloorArg(double);
void testFloorImm(double);
void testFloorMem(double);
void testFloorFloorArg(double);
void testCeilFloorArg(double);
void testFloorIToD64(int64_t);
void testFloorIToD32(int64_t);
void testFloorArg(float);
void testFloorImm(float);
void testFloorMem(float);
void testFloorFloorArg(float);
void testCeilFloorArg(float);
void testFloorArgWithUselessDoubleConversion(float);
void testFloorArgWithEffectfulDoubleConversion(float);
double correctSqrt(double value);
void testSqrtArg(double);
void testSqrtImm(double);
void testSqrtMem(double);
void testSqrtArg(float);
void testSqrtImm(float);
void testSqrtMem(float);
void testSqrtArgWithUselessDoubleConversion(float);
void testSqrtArgWithEffectfulDoubleConversion(float);
void testCompareTwoFloatToDouble(float, float);
void testCompareOneFloatToDouble(float, double);
void testCompareFloatToDoubleThroughPhi(float, float);
void testDoubleToFloatThroughPhi(float value);
void testReduceFloatToDoubleValidates();
void testDoubleProducerPhiToFloatConversion(float value);
void testDoubleProducerPhiToFloatConversionWithDoubleConsumer(float value);
void testDoubleProducerPhiWithNonFloatConst(float value, double constValue);
void testDoubleArgToInt64BitwiseCast(double value);
void testDoubleImmToInt64BitwiseCast(double value);
void testTwoBitwiseCastOnDouble(double value);
void testBitwiseCastOnDoubleInMemory(double value);
void testBitwiseCastOnDoubleInMemoryIndexed(double value);
void testInt64BArgToDoubleBitwiseCast(int64_t value);
void testInt64BImmToDoubleBitwiseCast(int64_t value);
void testTwoBitwiseCastOnInt64(int64_t value);
void testBitwiseCastOnInt64InMemory(int64_t value);
void testBitwiseCastOnInt64InMemoryIndexed(int64_t value);
void testFloatImmToInt32BitwiseCast(float value);
void testBitwiseCastOnFloatInMemory(float value);
void testInt32BArgToFloatBitwiseCast(int32_t value);
void testInt32BImmToFloatBitwiseCast(int32_t value);
void testTwoBitwiseCastOnInt32(int32_t value);
void testBitwiseCastOnInt32InMemory(int32_t value);
void testConvertDoubleToFloatArg(double value);
void testConvertDoubleToFloatImm(double value);
void testConvertDoubleToFloatMem(double value);
void testConvertFloatToDoubleArg(float value);
void testConvertFloatToDoubleImm(float value);
void testConvertFloatToDoubleMem(float value);
void testConvertDoubleToFloatToDouble(double value);
void testConvertDoubleToFloatToDoubleToFloat(double value);
void testConvertDoubleToFloatEqual(double value);
void testLoadFloatConvertDoubleConvertFloatStoreFloat(float value);
void testFroundArg(double value);
void testFroundMem(double value);
void testIToD64Arg();
void testIToF64Arg();
void testIToD32Arg();
void testIToF32Arg();
void testIToD64Mem();
void testIToF64Mem();
void testIToD32Mem();
void testIToF32Mem();
void testIToD64Imm(int64_t value);
void testIToF64Imm(int64_t value);
void testIToD32Imm(int32_t value);
void testIToF32Imm(int32_t value);
void testIToDReducedToIToF64Arg();
void testIToDReducedToIToF32Arg();
void testStoreZeroReg();
void testStore32(int value);
void testStoreConstant(int value);
void testStoreConstantPtr(intptr_t value);
void testStore8Arg();
void testStore8Imm();
void testStorePartial8BitRegisterOnX86();
void testStore16Arg();
void testStore16Imm();
void testTrunc(int64_t value);
void testAdd1(int value);
void testAdd1Ptr(intptr_t value);
void testNeg32(int32_t value);
void testNegPtr(intptr_t value);
void testStoreAddLoad32(int amount);
void testStoreRelAddLoadAcq32(int amount);
void testStoreAddLoadImm32(int amount);
void testStoreAddLoad8(int amount, B3::Opcode loadOpcode);
void testStoreRelAddLoadAcq8(int amount, B3::Opcode loadOpcode);
void testStoreRelAddFenceLoadAcq8(int amount, B3::Opcode loadOpcode);
void testStoreAddLoadImm8(int amount, B3::Opcode loadOpcode);
void testStoreAddLoad16(int amount, B3::Opcode loadOpcode);
void testStoreRelAddLoadAcq16(int amount, B3::Opcode loadOpcode);
void testStoreAddLoadImm16(int amount, B3::Opcode loadOpcode);
void testStoreAddLoad64(int amount);
void testStoreRelAddLoadAcq64(int amount);
void testStoreAddLoadImm64(int64_t amount);
void testStoreAddLoad32Index(int amount);
void testStoreAddLoadImm32Index(int amount);
void testStoreAddLoad8Index(int amount, B3::Opcode loadOpcode);
void testStoreAddLoadImm8Index(int amount, B3::Opcode loadOpcode);
void testStoreAddLoad16Index(int amount, B3::Opcode loadOpcode);
void testStoreAddLoadImm16Index(int amount, B3::Opcode loadOpcode);
void testStoreAddLoad64Index(int amount);
void testStoreAddLoadImm64Index(int64_t amount);
void testStoreSubLoad(int amount);
void testStoreAddLoadInterference(int amount);
void testStoreAddAndLoad(int amount, int mask);
void testStoreNegLoad32(int32_t value);
void testStoreNegLoadPtr(intptr_t value);
void testAdd1Uncommuted(int value);
void testLoadOffset();
void testLoadOffsetNotConstant();
void testLoadOffsetUsingAdd();
void testLoadOffsetUsingAddInterference();
void testLoadOffsetUsingAddNotConstant();
void testLoadAddrShift(unsigned shift);
void testFramePointer();
void testOverrideFramePointer();
void testStackSlot();
void testLoadFromFramePointer();
void testStoreLoadStackSlot(int value);
void testStoreFloat(double input);
void testStoreDoubleConstantAsFloat(double input);
void testSpillGP();
void testSpillFP();
void testInt32ToDoublePartialRegisterStall();
void testInt32ToDoublePartialRegisterWithoutStall();
void testBranch();
void testBranchPtr();
void testDiamond();
void testBranchNotEqual();
void testBranchNotEqualCommute();
void testBranchNotEqualNotEqual();
void testBranchEqual();
void testBranchEqualEqual();
void testBranchEqualCommute();
void testBranchEqualEqual1();
void testBranchEqualOrUnorderedArgs(double, double);
void testBranchEqualOrUnorderedArgs(float, float);
void testBranchNotEqualAndOrderedArgs(double, double);
void testBranchNotEqualAndOrderedArgs(float, float);
void testBranchEqualOrUnorderedDoubleArgImm(double, double);
void testBranchEqualOrUnorderedFloatArgImm(float, float);
void testBranchEqualOrUnorderedDoubleImms(double, double);
void testBranchEqualOrUnorderedFloatImms(float, float);
void testBranchEqualOrUnorderedFloatWithUselessDoubleConversion(float, float);
void testBranchFold(int value);
void testDiamondFold(int value);
void testBranchNotEqualFoldPtr(intptr_t value);
void testBranchEqualFoldPtr(intptr_t value);
void testBranchLoadPtr();
void testBranchLoad32();
void testBranchLoad8S();
void testBranchLoad8Z();
void testBranchLoad16S();
void testBranchLoad16Z();
void testBranch8WithLoad8ZIndex();
void testComplex(unsigned numVars, unsigned numConstructs);
void testBranchBitTest32TmpImm(uint32_t value, uint32_t imm);
void testBranchBitTest32AddrImm(uint32_t value, uint32_t imm);
void testBranchBitTest32TmpTmp(uint32_t value, uint32_t value2);
void testBranchBitTest64TmpTmp(uint64_t value, uint64_t value2);
void testBranchBitTest64AddrTmp(uint64_t value, uint64_t value2);
void testBranchBitTestNegation(uint64_t value, uint64_t value2);
void testBranchBitTestNegation2(uint64_t value, uint64_t value2);
void testSimplePatchpoint();
void testSimplePatchpointWithoutOuputClobbersGPArgs();
void testSimplePatchpointWithOuputClobbersGPArgs();
void testSimplePatchpointWithoutOuputClobbersFPArgs();
void testSimplePatchpointWithOuputClobbersFPArgs();
void testPatchpointWithEarlyClobber();
void testPatchpointCallArg();
void testPatchpointFixedRegister();
void testPatchpointAny(ValueRep rep);
void testPatchpointGPScratch();
void testPatchpointFPScratch();
void testPatchpointLotsOfLateAnys();
void testPatchpointAnyImm(ValueRep rep);
void testPatchpointManyWarmAnyImms();
void testPatchpointManyColdAnyImms();
void testPatchpointWithRegisterResult();
void testPatchpointWithStackArgumentResult();
void testPatchpointWithAnyResult();
void testSimpleCheck();
void testCheckFalse();
void testCheckTrue();
void testCheckLessThan();
void testCheckMegaCombo();
void testCheckTrickyMegaCombo();
void testCheckTwoMegaCombos();
void testCheckTwoNonRedundantMegaCombos();
void testCheckAddImm();
void testCheckAddImmCommute();
void testCheckAddImmSomeRegister();
void testCheckAdd();
void testCheckAdd64();
void testCheckAddFold(int, int);
void testCheckAddFoldFail(int, int);
void test42();
void testLoad42();
void testLoadAcq42();
void testLoadWithOffsetImpl(int32_t offset64, int32_t offset32);
void testLoadOffsetImm9Max();
void testLoadOffsetImm9MaxPlusOne();
void testLoadOffsetImm9MaxPlusTwo();
void testLoadOffsetImm9Min();
void testLoadOffsetImm9MinMinusOne();
void testLoadOffsetScaledUnsignedImm12Max();
void testLoadOffsetScaledUnsignedOverImm12Max();
void testAddTreeArg32(int32_t);
void testMulTreeArg32(int32_t);
void testArg(int argument);
void testReturnConst64(int64_t value);
void testReturnVoid();
void testLoadZeroExtendIndexAddress();
void testLoadSignExtendIndexAddress();
void testStoreZeroExtendIndexAddress();
void testStoreSignExtendIndexAddress();
void testAddArg(int);
void testAddArgs(int, int);
void testAddArgImm(int, int);
void testAddImmArg(int, int);
void testAddArgMem(int64_t, int64_t);
void testAddMemArg(int64_t, int64_t);
void testAddImmMem(int64_t, int64_t);
void testAddArg32(int);
void testAddArgs32(int, int);
void testAddArgMem32(int32_t, int32_t);
void testAddMemArg32(int32_t, int32_t);
void testAddImmMem32(int32_t, int32_t);
void testAddNeg1(int, int);
void testAddNeg2(int, int);
void testAddArgZeroImmZDef();
void testAddLoadTwice();
void testAddArgDouble(double);
void testCheckAddArgumentAliasing64();
void testCheckAddArgumentAliasing32();
void testCheckAddSelfOverflow64();
void testCheckAddSelfOverflow32();
void testCheckAddRemoveCheckWithSExt8(int8_t);
void testCheckAddRemoveCheckWithSExt16(int16_t);
void testCheckAddRemoveCheckWithSExt32(int32_t);
void testCheckAddRemoveCheckWithZExt32(int32_t);
void testCheckSubImm();
void testCheckSubBadImm();
void testCheckSub();
void testCheckSubBitAnd();
double doubleSub(double, double);
void testCheckSub64();
void testCheckSubFold(int, int);
void testCheckSubFoldFail(int, int);
void testCheckNeg();
void testCheckNeg64();
void testCheckMul();
void testCheckMulMemory();
void testCheckMul2();
void testCheckMul64();
void testCheckMulFold(int, int);
void testCheckMulFoldFail(int, int);
void testAddArgsDouble(double, double);
void testAddArgImmDouble(double, double);
void testAddImmArgDouble(double, double);
void testAddImmsDouble(double, double);
void testAddArgFloat(float);
void testAddArgsFloat(float, float);
void testAddFPRArgsFloat(float, float);
void testAddArgImmFloat(float, float);
void testAddImmArgFloat(float, float);
void testAddImmsFloat(float, float);
void testAddArgFloatWithUselessDoubleConversion(float);
void testAddArgsFloatWithUselessDoubleConversion(float, float);
void testAddArgsFloatWithEffectfulDoubleConversion(float, float);
void testAddMulMulArgs(int64_t, int64_t, int64_t c);
void testMulArg(int);
void testMulArgStore(int);
void testMulAddArg(int);
void testMulArgs(int, int);
void testMulArgNegArg(int, int);
void testCheckMulArgumentAliasing64();
void testCheckMulArgumentAliasing32();
void testCheckMul64SShr();
void testCompareImpl(B3::Opcode opcode, int64_t left, int64_t right);
void testCompare(B3::Opcode opcode, int64_t left, int64_t right);
void testEqualDouble(double left, double right, bool result);
void testCallSimple(int, int);
void testCallRare(int, int);
void testCallRareLive(int, int, int c);
void testCallSimplePure(int, int);
void testCallFunctionWithHellaArguments();
void testCallFunctionWithHellaArguments2();
void testCallFunctionWithHellaArguments3();
void testReturnDouble(double value);
void testReturnFloat(float value);
void testMulNegArgArg(int, int);
void testMulArgImm(int64_t, int64_t);
void testMulImmArg(int, int);
void testMulArgs32(int, int);
void testMulArgs32SignExtend();
void testMulArgs32ZeroExtend();
void testMulImm32SignExtend(const int, int);
void testMulLoadTwice();
void testMulAddArgsLeft();
void testMulAddArgsRight();
void testMulAddArgsLeft32();
void testMulAddArgsRight32();
void testMulAddSignExtend32ArgsLeft();
void testMulAddSignExtend32ArgsRight();
void testMulAddZeroExtend32ArgsLeft();
void testMulAddZeroExtend32ArgsRight();
void testMulSubArgsLeft();
void testMulSubArgsRight();
void testMulSubArgsLeft32();
void testMulSubArgsRight32();
void testMulSubSignExtend32();
void testMulSubZeroExtend32();
void testMulNegArgs();
void testMulNegArgs32();
void testMulNegSignExtend32();
void testMulNegZeroExtend32();
void testMulArgDouble(double);
void testMulArgsDouble(double, double);
void testCallSimpleDouble(double, double);
void testCallSimpleFloat(float, float);
void testCallFunctionWithHellaDoubleArguments();
void testCallFunctionWithHellaFloatArguments();
void testLinearScanWithCalleeOnStack();
void testChillDiv(int num, int den, int res);
void testChillDivTwice(int num1, int den1, int num2, int den2, int res);
void testChillDiv64(int64_t num, int64_t den, int64_t res);
void testModArg(int64_t value);
void testModArgs(int64_t numerator, int64_t denominator);
void testModImms(int64_t numerator, int64_t denominator);
void testMulArgImmDouble(double, double);
void testMulImmArgDouble(double, double);
void testMulImmsDouble(double, double);
void testMulArgFloat(float);
void testMulArgsFloat(float, float);
void testMulArgImmFloat(float, float);
void testMulImmArgFloat(float, float);
void testMulImmsFloat(float, float);
void testMulArgFloatWithUselessDoubleConversion(float);
void testMulArgsFloatWithUselessDoubleConversion(float, float);
void testMulArgsFloatWithEffectfulDoubleConversion(float, float);
void testDivArgDouble(double);
void testDivArgsDouble(double, double);
void testDivArgImmDouble(double, double);
void testDivImmArgDouble(double, double);
void testDivImmsDouble(double, double);
void testDivArgFloat(float);
void testDivArgsFloat(float, float);
void testDivArgImmFloat(float, float);
void testModArg32(int32_t value);
void testModArgs32(int32_t numerator, int32_t denominator);
void testModImms32(int32_t numerator, int32_t denominator);
void testChillModArg(int64_t value);
void testChillModArgs(int64_t numerator, int64_t denominator);
void testChillModImms(int64_t numerator, int64_t denominator);
void testChillModArg32(int32_t value);
void testChillModArgs32(int32_t numerator, int32_t denominator);
void testChillModImms32(int32_t numerator, int32_t denominator);
void testLoopWithMultipleHeaderEdges();
void testSwitch(unsigned degree);
void testSwitch(unsigned degree, unsigned gap);
void testSwitchSameCaseAsDefault();
void testSwitchChillDiv(unsigned degree, unsigned gap);
void testSwitchTargettingSameBlock();
void testSwitchTargettingSameBlockFoldPathConstant();
void testTruncFold(int64_t value);
void testZExt32(int32_t value);
void testZExt32Fold(int32_t value);
void testSExt32(int32_t value);
void testSExt32Fold(int32_t value);
void testTruncZExt32(int32_t value);
void testPinRegisters();
void testX86LeaAddAddShlLeft();
void testX86LeaAddAddShlRight();
void testX86LeaAddAdd();
void testX86LeaAddShlRight();
void testX86LeaAddShlLeftScale1();
void testX86LeaAddShlLeftScale2();
void testX86LeaAddShlLeftScale4();
void testX86LeaAddShlLeftScale8();
void testAddShl32();
void testAddShl64();
void testAddShl65();
void testReduceStrengthReassociation(bool flip);
void testLoadBaseIndexShift2();
void testLoadBaseIndexShift32();
void testOptimizeMaterialization();
void testLICMPure();
void testLICMPureSideExits();
void testTruncSExt32(int32_t value);
void testSExt8(int32_t value);
void testSExt8Fold(int32_t value);
void testSExt8SExt8(int32_t value);
void testSExt8SExt16(int32_t value);
void testSExt8BitAnd(int32_t value, int32_t mask);
void testBitAndSExt8(int32_t value, int32_t mask);
void testSExt16(int32_t value);
void testSExt16Fold(int32_t value);
void testSExt16SExt16(int32_t value);
void testSExt16SExt8(int32_t value);
void testLICMPureWritesPinned();
void testLICMPureWrites();
void testLICMReadsLocalState();
void testLICMReadsPinned();
void testLICMReads();
void testLICMPureNotBackwardsDominant();
void testLICMPureFoiledByChild();
void testLICMPureNotBackwardsDominantFoiledByChild();
void testLICMExitsSideways();
void testLICMWritesLocalState();
void testLICMWrites();
void testLICMFence();
void testLICMWritesPinned();
void testLICMControlDependent();
void testLICMControlDependentNotBackwardsDominant();
void testLICMControlDependentSideExits();
void testLICMReadsPinnedWritesPinned();
void testLICMReadsWritesDifferentHeaps();
void testLICMReadsWritesOverlappingHeaps();
void testLICMDefaultCall();
void testDepend32();
void testDepend64();
void testWasmBoundsCheck(unsigned offset);
void testWasmAddress();
void testFastTLSLoad();
void testFastTLSStore();
void testDoubleLiteralComparison(double, double);
void testFloatEqualOrUnorderedFolding();
void testFloatEqualOrUnorderedFoldingNaN();
void testFloatEqualOrUnorderedDontFold();
void testSExt16BitAnd(int32_t value, int32_t mask);
void testBitAndSExt16(int32_t value, int32_t mask);
void testSExt32BitAnd(int32_t value, int32_t mask);
void testShuffleDoesntTrashCalleeSaves();
void testDemotePatchpointTerminal();
void testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead();
void testInfiniteLoopDoesntCauseBadHoisting();
void testDivImmArgFloat(float, float);
void testDivImmsFloat(float, float);
void testModArgDouble(double);
void testModArgsDouble(double, double);
void testModArgImmDouble(double, double);
void testModImmArgDouble(double, double);
void testModImmsDouble(double, double);
void testModArgFloat(float);
void testModArgsFloat(float, float);
void testModArgImmFloat(float, float);
void testModImmArgFloat(float, float);
void testModImmsFloat(float, float);
void testDivArgFloatWithUselessDoubleConversion(float);
void testDivArgsFloatWithUselessDoubleConversion(float, float);
void testDivArgsFloatWithEffectfulDoubleConversion(float, float);
void testUDivArgsInt32(uint32_t, uint32_t);
void testUDivArgsInt64(uint64_t, uint64_t);
void testUModArgsInt32(uint32_t, uint32_t);
void testUModArgsInt64(uint64_t, uint64_t);
void testSubArg(int);
void testSubArgs(int, int);
void testSubArgImm(int64_t, int64_t);
void testSubNeg(int, int);
void testNegSub(int, int);
void testNegValueSubOne(int);
void testSubSub(int, int, int c);
void testSubSub2(int, int, int c);
void testSubAdd(int, int, int c);
void testSubFirstNeg(int, int);
void testSubImmArg(int, int);
void testSubArgMem(int64_t, int64_t);
void testSubMemArg(int64_t, int64_t);
void testSubImmMem(int64_t, int64_t);
void testSubMemImm(int64_t, int64_t);
void testSubArgs32(int, int);
void testSubArgs32ZeroExtend(int, int);
void testSubArgImm32(int, int);
void testSubImmArg32(int, int);
void testSubMemArg32(int32_t, int32_t);
void testSubArgMem32(int32_t, int32_t);
void testSubImmMem32(int32_t, int32_t);
void testSubMemImm32(int32_t, int32_t);
void testNegValueSubOne32(int);
void testNegMulArgImm(int64_t, int64_t);
void testSubMulMulArgs(int64_t, int64_t, int64_t c);
void testSubArgDouble(double);
void testSubArgsDouble(double, double);
void testSubArgImmDouble(double, double);
void testSubImmArgDouble(double, double);
void testSubImmsDouble(double, double);
void testSubArgFloat(float);
void testSubArgsFloat(float, float);
void testSubArgImmFloat(float, float);
void testSubImmArgFloat(float, float);
void testSubImmsFloat(float, float);
void testSubArgFloatWithUselessDoubleConversion(float);
void testSubArgsFloatWithUselessDoubleConversion(float, float);
void testSubArgsFloatWithEffectfulDoubleConversion(float, float);
void testTernarySubInstructionSelection(B3::Opcode valueModifier, Type valueType, Air::Opcode expectedOpcode);
void testNegDouble(double);
void testNegFloat(float);
void testNegFloatWithUselessDoubleConversion(float);

void addArgTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addBitTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addCallTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addSExtTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addSShrShTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addShrTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addAtomicTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addLoadTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);
void addTupleTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>&);

bool shouldRun(const char* filter, const char* testName);

void testLoadPreIndex32();
void testLoadPreIndex64();
void testLoadPostIndex32();
void testLoadPostIndex64();

void testStorePreIndex32();
void testStorePreIndex64();
void testStorePostIndex32();
void testStorePostIndex64();

void testFloatMaxMin();
void testDoubleMaxMin();

void testWasmAddressDoesNotCSE();
void testStoreAfterClobberExitsSideways();
void testStoreAfterClobberDifferentWidth();
void testStoreAfterClobberDifferentWidthSuccessor();
void testStoreAfterClobberExitsSidewaysSuccessor();

#endif // ENABLE(B3_JIT)
