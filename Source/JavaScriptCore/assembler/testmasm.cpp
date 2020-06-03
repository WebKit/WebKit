/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "CPU.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "InitializeThreading.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "StackAlignment.h"
#include <limits>
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/NumberOfCores.h>
#include <wtf/PtrTag.h>
#include <wtf/Threading.h>
#include <wtf/text/StringCommon.h>

// We don't have a NO_RETURN_DUE_TO_EXIT, nor should we. That's ridiculous.
static bool hiddenTruthBecauseNoReturnIsStupid() { return true; }

static void usage()
{
    dataLog("Usage: testmasm [<filter>]\n");
    if (hiddenTruthBecauseNoReturnIsStupid())
        exit(1);
}

#if ENABLE(JIT)

#if ENABLE(MASM_PROBE)
namespace WTF {

static void printInternal(PrintStream& out, void* value)
{
    out.printf("%p", value);
}

} // namespace WTF
#endif // ENABLE(MASM_PROBE)

namespace JSC {
namespace Probe {

JS_EXPORT_PRIVATE void* probeStateForContext(Probe::Context&);

} // namespace Probe
} // namespace JSC

using namespace JSC;

namespace {

#if ENABLE(MASM_PROBE)
using CPUState = Probe::CPUState;
#endif

Lock crashLock;

typedef WTF::Function<void(CCallHelpers&)> Generator;

template<typename T> T nextID(T id) { return static_cast<T>(id + 1); }

#define TESTWORD64 0x0c0defefebeef000
#define TESTWORD32 0x0beef000

#define testWord32(x) (TESTWORD32 + static_cast<uint32_t>(x))
#define testWord64(x) (TESTWORD64 + static_cast<uint64_t>(x))

#if USE(JSVALUE64)
#define testWord(x) testWord64(x)
#else
#define testWord(x) testWord32(x)
#endif

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK_EQ(_actual, _expected) do {                               \
        if ((_actual) == (_expected))                                   \
            break;                                                      \
        crashLock.lock();                                               \
        dataLog("FAILED while testing " #_actual ": expected: ", _expected, ", actual: ", _actual, "\n"); \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "CHECK_EQ("#_actual ", " #_expected ")"); \
        CRASH();                                                        \
    } while (false)

#define CHECK_NOT_EQ(_actual, _expected) do {                               \
        if ((_actual) != (_expected))                                   \
            break;                                                      \
        crashLock.lock();                                               \
        dataLog("FAILED while testing " #_actual ": expected not: ", _expected, ", actual: ", _actual, "\n"); \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "CHECK_NOT_EQ("#_actual ", " #_expected ")"); \
        CRASH();                                                        \
    } while (false)

#if ENABLE(MASM_PROBE)
bool isPC(MacroAssembler::RegisterID id)
{
#if CPU(ARM_THUMB2)
    return id == ARMRegisters::pc;
#else
    UNUSED_PARAM(id);
    return false;
#endif
}

bool isSP(MacroAssembler::RegisterID id)
{
    return id == MacroAssembler::stackPointerRegister;
}

bool isFP(MacroAssembler::RegisterID id)
{
    return id == MacroAssembler::framePointerRegister;
}

bool isSpecialGPR(MacroAssembler::RegisterID id)
{
    if (isPC(id) || isSP(id) || isFP(id))
        return true;
#if CPU(ARM64)
    if (id == ARM64Registers::x18)
        return true;
#elif CPU(MIPS)
    if (id == MIPSRegisters::zero || id == MIPSRegisters::k0 || id == MIPSRegisters::k1)
        return true;
#endif
    return false;
}
#endif // ENABLE(MASM_PROBE)

MacroAssemblerCodeRef<JSEntryPtrTag> compile(Generator&& generate)
{
    CCallHelpers jit;
    generate(jit);
    LinkBuffer linkBuffer(jit, nullptr);
    return FINALIZE_CODE(linkBuffer, JSEntryPtrTag, "testmasm compilation");
}

template<typename T, typename... Arguments>
T invoke(const MacroAssemblerCodeRef<JSEntryPtrTag>& code, Arguments... arguments)
{
    void* executableAddress = untagCFunctionPtr<JSEntryPtrTag>(code.code().executableAddress());
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(executableAddress);
    return function(arguments...);
}

template<typename T, typename... Arguments>
T compileAndRun(Generator&& generator, Arguments... arguments)
{
    return invoke<T>(compile(WTFMove(generator)), arguments...);
}

void emitFunctionPrologue(CCallHelpers& jit)
{
    jit.emitFunctionPrologue();
#if CPU(ARM_THUMB2)
    // MacroAssemblerARMv7 uses r6 as a temporary register, which is a
    // callee-saved register, see 5.1.1 of the Procedure Call Standard for
    // the ARM Architecture.
    // http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042f/IHI0042F_aapcs.pdf
    jit.push(ARMRegisters::r6);
#endif
}

void emitFunctionEpilogue(CCallHelpers& jit)
{
#if CPU(ARM_THUMB2)
    jit.pop(ARMRegisters::r6);
#endif
    jit.emitFunctionEpilogue();
}

void testSimple()
{
    CHECK_EQ(compileAndRun<int>([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    }), 42);
}

void testGetEffectiveAddress(size_t pointer, ptrdiff_t length, int32_t offset, CCallHelpers::Scale scale)
{
    CHECK_EQ(compileAndRun<size_t>([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(pointer)), GPRInfo::regT0);
        jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(length)), GPRInfo::regT1);
        jit.getEffectiveAddress(CCallHelpers::BaseIndex(GPRInfo::regT0, GPRInfo::regT1, scale, offset), GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    }), pointer + offset + (static_cast<size_t>(1) << static_cast<int>(scale)) * length);
}

// branchTruncateDoubleToInt32(), when encountering Infinity, -Infinity or a
// Nan, should either yield 0 in dest or fail.
void testBranchTruncateDoubleToInt32(double val, int32_t expected)
{
    const uint64_t valAsUInt = *reinterpret_cast<uint64_t*>(&val);
#if CPU(BIG_ENDIAN)
    const bool isBigEndian = true;
#else
    const bool isBigEndian = false;
#endif
    CHECK_EQ(compileAndRun<int>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.subPtr(CCallHelpers::TrustedImm32(stackAlignmentBytes()), MacroAssembler::stackPointerRegister);
        if (isBigEndian) {
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt >> 32),
                MacroAssembler::stackPointerRegister);
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt & 0xffffffff),
                MacroAssembler::Address(MacroAssembler::stackPointerRegister, 4));
        } else {
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt & 0xffffffff),
                MacroAssembler::stackPointerRegister);
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt >> 32),
                MacroAssembler::Address(MacroAssembler::stackPointerRegister, 4));
        }
        jit.loadDouble(MacroAssembler::stackPointerRegister, FPRInfo::fpRegT0);

        MacroAssembler::Jump done;
        done = jit.branchTruncateDoubleToInt32(FPRInfo::fpRegT0, GPRInfo::returnValueGPR, MacroAssembler::BranchIfTruncateSuccessful);

        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);

        done.link(&jit);
        jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentBytes()), MacroAssembler::stackPointerRegister);
        emitFunctionEpilogue(jit);
        jit.ret();
    }), expected);
}


static Vector<double> doubleOperands()
{
    return Vector<double> {
        0,
        -0,
        1,
        -1,
        42,
        -42,
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
}


#if CPU(X86) || CPU(X86_64) || CPU(ARM64)
static Vector<float> floatOperands()
{
    return Vector<float> {
        0,
        -0,
        1,
        -1,
        42,
        -42,
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
}
#endif

static Vector<int32_t> int32Operands()
{
    return Vector<int32_t> {
        0,
        1,
        -1,
        2,
        -2,
        42,
        -42,
        64,
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int32_t>::min(),
    };
}

#if CPU(X86_64) || CPU(ARM64)
static Vector<int64_t> int64Operands()
{
    return Vector<int64_t> {
        0,
        1,
        -1,
        2,
        -2,
        42,
        -42,
        64,
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::min(),
    };
}
#endif

#if CPU(X86_64)
void testBranchTestBit32RegReg()
{
    for (auto value : int32Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit32(MacroAssembler::NonZero, GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
            jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int32Operands())
            CHECK_EQ(invoke<int>(test, value, value2), (value>>(value2%32))&1);
    }
}

void testBranchTestBit32RegImm()
{
    for (auto value : int32Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit32(MacroAssembler::NonZero, GPRInfo::argumentGPR0, CCallHelpers::TrustedImm32(value));
            jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int32Operands())
            CHECK_EQ(invoke<int>(test, value2), (value2>>(value%32))&1);
    }
}

void testBranchTestBit32AddrImm()
{
    for (auto value : int32Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit32(MacroAssembler::NonZero, MacroAssembler::Address(GPRInfo::argumentGPR0, 0), CCallHelpers::TrustedImm32(value));
            jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int32Operands())
            CHECK_EQ(invoke<int>(test, &value2), (value2>>(value%32))&1);
    }
}

void testBranchTestBit64RegReg()
{
    for (auto value : int64Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit64(MacroAssembler::NonZero, GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
            jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm64(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int64Operands())
            CHECK_EQ(invoke<long int>(test, value, value2), (value>>(value2%64))&1);
    }
}

void testBranchTestBit64RegImm()
{
    for (auto value : int64Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit64(MacroAssembler::NonZero, GPRInfo::argumentGPR0, CCallHelpers::TrustedImm32(value));
            jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm64(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int64Operands())
            CHECK_EQ(invoke<long int>(test, value2), (value2>>(value%64))&1);
    }
}

void testBranchTestBit64AddrImm()
{
    for (auto value : int64Operands()) {
        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            auto branch = jit.branchTestBit64(MacroAssembler::NonZero, MacroAssembler::Address(GPRInfo::argumentGPR0, 0), CCallHelpers::TrustedImm32(value));
            jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::returnValueGPR);
            auto done = jit.jump();
            branch.link(&jit);
            jit.move(CCallHelpers::TrustedImm64(1), GPRInfo::returnValueGPR);
            done.link(&jit);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int64Operands())
            CHECK_EQ(invoke<long int>(test, &value2), (value2>>(value%64))&1);
    }
}

#endif

#if CPU(X86_64) || CPU(ARM64)
void testClearBit64()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg scratchGPR = GPRInfo::argumentGPR2;
        jit.clearBit64(GPRInfo::argumentGPR1, GPRInfo::argumentGPR0, scratchGPR);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    constexpr unsigned bitsInWord = sizeof(uint64_t) * 8;

    for (unsigned i = 0; i < bitsInWord; ++i) {
        uint64_t word = std::numeric_limits<uint64_t>::max();
        constexpr uint64_t one = 1;
        CHECK_EQ(invoke<uint64_t>(test, word, i), (word & ~(one << i)));
    }

    for (unsigned i = 0; i < bitsInWord; ++i) {
        uint64_t word = 0;
        CHECK_EQ(invoke<uint64_t>(test, word, i), 0);
    }
}

void testClearBits64WithMask()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.clearBits64WithMask(GPRInfo::argumentGPR1, GPRInfo::argumentGPR0);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto value : int64Operands()) {
        uint64_t word = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<uint64_t>(test, word, value), (word & ~value));
    }

    for (auto value : int64Operands()) {
        uint64_t word = 0;
        CHECK_EQ(invoke<uint64_t>(test, word, value), 0);
    }

#if ENABLE(MASM_PROBE)
    uint64_t savedMask = 0;
    auto test2 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.probe([&] (Probe::Context& context) {
            savedMask = context.gpr<uint64_t>(GPRInfo::argumentGPR1);
        });

        jit.clearBits64WithMask(GPRInfo::argumentGPR1, GPRInfo::argumentGPR0, CCallHelpers::ClearBitsAttributes::MustPreserveMask);

        jit.probe([&] (Probe::Context& context) {
            CHECK_EQ(savedMask, context.gpr<uint64_t>(GPRInfo::argumentGPR1));
        });
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto value : int64Operands()) {
        uint64_t word = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<uint64_t>(test2, word, value), (word & ~value));
    }

    for (auto value : int64Operands()) {
        uint64_t word = 0;
        CHECK_EQ(invoke<uint64_t>(test2, word, value), 0);
    }
#endif
}

void testClearBits64WithMaskTernary()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.move(GPRInfo::argumentGPR0, GPRInfo::argumentGPR2);
        jit.move(GPRInfo::argumentGPR1, GPRInfo::argumentGPR3);
        jit.clearBits64WithMask(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto value : int64Operands()) {
        uint64_t word = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<uint64_t>(test, word, value), (word & ~value));
    }

    for (auto value : int64Operands()) {
        uint64_t word = 0;
        CHECK_EQ(invoke<uint64_t>(test, word, value), 0);
    }

#if ENABLE(MASM_PROBE)
    uint64_t savedMask = 0;
    auto test2 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.move(GPRInfo::argumentGPR0, GPRInfo::argumentGPR2);
        jit.move(GPRInfo::argumentGPR1, GPRInfo::argumentGPR3);

        jit.probe([&] (Probe::Context& context) {
            savedMask = context.gpr<uint64_t>(GPRInfo::argumentGPR2);
        });

        jit.clearBits64WithMask(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3, GPRInfo::returnValueGPR, CCallHelpers::ClearBitsAttributes::MustPreserveMask);

        jit.probe([&] (Probe::Context& context) {
            CHECK_EQ(savedMask, context.gpr<uint64_t>(GPRInfo::argumentGPR2));
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto value : int64Operands()) {
        uint64_t word = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<uint64_t>(test2, word, value), (word & ~value));
    }

    for (auto value : int64Operands()) {
        uint64_t word = 0;
        CHECK_EQ(invoke<uint64_t>(test2, word, value), 0);
    }
#endif
}

static void testCountTrailingZeros64Impl(bool wordCanBeZero)
{
    auto test = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        if (wordCanBeZero)
            jit.countTrailingZeros64(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        else
            jit.countTrailingZeros64WithoutNullCheck(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    constexpr size_t numberOfBits = sizeof(uint64_t) * 8;

    auto expectedNumberOfTrailingZeros = [=] (uint64_t word) -> size_t {
        size_t count = 0;
        for (size_t i = 0; i < numberOfBits; ++i) {
            if (word & 1)
                break;
            word >>= 1;
            count++;
        }
        return count;
    };

    for (auto word : int64Operands()) {
        if (!wordCanBeZero && !word)
            continue;
        CHECK_EQ(invoke<size_t>(test, word), expectedNumberOfTrailingZeros(word));
    }

    for (size_t i = 0; i < numberOfBits; ++i) {
        uint64_t one = 1;
        uint64_t word = one << i;
        CHECK_EQ(invoke<size_t>(test, word), i);
    }
}

void testCountTrailingZeros64()
{
    bool wordCanBeZero = true;
    testCountTrailingZeros64Impl(wordCanBeZero);
}

void testCountTrailingZeros64WithoutNullCheck()
{
    bool wordCanBeZero = false;
    testCountTrailingZeros64Impl(wordCanBeZero);
}

void testStore64Imm64AddressPointer()
{
    auto doTest = [] (int64_t value) {
        int64_t dest;
        void* destAddress = &dest;

        auto test = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store64(CCallHelpers::TrustedImm64(value), destAddress);
            emitFunctionEpilogue(jit);
            jit.ret();
        });

        invoke<size_t>(test);
        CHECK_EQ(dest, value);
    };
    
    for (auto value : int64Operands())
        doTest(value);

    doTest(0x98765555AAAA4321);
    doTest(0xAAAA432198765555);
}

#endif // CPU(X86_64) || CPU(ARM64)

void testCompareDouble(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;

    auto compareDouble = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(-1), GPRInfo::returnValueGPR);
        jit.compareDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto compareDoubleGeneric = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
        auto jump = jit.branchDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
        jump.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto expectedResult = [&, condition] (double a, double b) -> int {
        auto isUnordered = [] (double x) {
            return x != x;
        };
        switch (condition) {
        case MacroAssembler::DoubleEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a == b);
        case MacroAssembler::DoubleNotEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a != b);
        case MacroAssembler::DoubleGreaterThanAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a > b);
        case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a >= b);
        case MacroAssembler::DoubleLessThanAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a < b);
        case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a <= b);
        case MacroAssembler::DoubleEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a == b);
        case MacroAssembler::DoubleNotEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a != b);
        case MacroAssembler::DoubleGreaterThanOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a > b);
        case MacroAssembler::DoubleGreaterThanOrEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a >= b);
        case MacroAssembler::DoubleLessThanOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a < b);
        case MacroAssembler::DoubleLessThanOrEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a <= b);
        } // switch
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto operands = doubleOperands();
    for (auto a : operands) {
        for (auto b : operands) {
            arg1 = a;
            arg2 = b;
            CHECK_EQ(invoke<int>(compareDouble), expectedResult(a, b));
            CHECK_EQ(invoke<int>(compareDoubleGeneric), expectedResult(a, b));
        }
    }
}

void testCompareDoubleSameArg(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;

    auto compareDouble = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.move(CCallHelpers::TrustedImm32(-1), GPRInfo::returnValueGPR);
        jit.compareDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto compareDoubleGeneric = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
        auto jump = jit.branchDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
        jump.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto expectedResult = [&, condition] (double a) -> int {
        auto isUnordered = [] (double x) {
            return x != x;
        };
        switch (condition) {
        case MacroAssembler::DoubleEqualAndOrdered:
            return !isUnordered(a) && (a == a);
        case MacroAssembler::DoubleNotEqualAndOrdered:
            return !isUnordered(a) && (a != a);
        case MacroAssembler::DoubleGreaterThanAndOrdered:
            return !isUnordered(a) && (a > a);
        case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
            return !isUnordered(a) && (a >= a);
        case MacroAssembler::DoubleLessThanAndOrdered:
            return !isUnordered(a) && (a < a);
        case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
            return !isUnordered(a) && (a <= a);
        case MacroAssembler::DoubleEqualOrUnordered:
            return isUnordered(a) || (a == a);
        case MacroAssembler::DoubleNotEqualOrUnordered:
            return isUnordered(a) || (a != a);
        case MacroAssembler::DoubleGreaterThanOrUnordered:
            return isUnordered(a) || (a > a);
        case MacroAssembler::DoubleGreaterThanOrEqualOrUnordered:
            return isUnordered(a) || (a >= a);
        case MacroAssembler::DoubleLessThanOrUnordered:
            return isUnordered(a) || (a < a);
        case MacroAssembler::DoubleLessThanOrEqualOrUnordered:
            return isUnordered(a) || (a <= a);
        } // switch
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto operands = doubleOperands();
    for (auto a : operands) {
        arg1 = a;
        CHECK_EQ(invoke<int>(compareDouble), expectedResult(a));
        CHECK_EQ(invoke<int>(compareDoubleGeneric), expectedResult(a));
    }
}

void testMul32WithImmediates()
{
    for (auto immediate : int32Operands()) {
        auto mul = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.mul32(CCallHelpers::TrustedImm32(immediate), GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value : int32Operands())
            CHECK_EQ(invoke<int>(mul, value), immediate * value);
    }
}

#if CPU(ARM64)
void testMul32SignExtend()
{
    for (auto value : int32Operands()) {
        auto mul = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.multiplySignExtend32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int32Operands())
            CHECK_EQ(invoke<long int>(mul, value, value2), ((long int) value) * ((long int) value2));
    }
}
#endif

#if CPU(X86) || CPU(X86_64) || CPU(ARM64)
void testCompareFloat(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;

    auto compareFloat = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(-1), GPRInfo::returnValueGPR);
        jit.compareFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto compareFloatGeneric = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
        auto jump = jit.branchFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
        jump.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto operands = floatOperands();
    for (auto a : operands) {
        for (auto b : operands) {
            arg1 = a;
            arg2 = b;
            CHECK_EQ(invoke<int>(compareFloat), invoke<int>(compareFloatGeneric));
        }
    }
}
#endif // CPU(X86) || CPU(X86_64) || CPU(ARM64)

#if CPU(X86_64) || CPU(ARM64)

template<typename T, typename SelectionType>
void testMoveConditionallyFloatingPoint(MacroAssembler::DoubleCondition condition, const MacroAssemblerCodeRef<JSEntryPtrTag>& testCode, T& arg1, T& arg2, const Vector<T> operands, SelectionType selectionA, SelectionType selectionB)
{
    auto expectedResult = [&, condition] (T a, T b) -> SelectionType {
        auto isUnordered = [] (double x) {
            return x != x;
        };
        switch (condition) {
        case MacroAssembler::DoubleEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a == b) ? selectionA : selectionB;
        case MacroAssembler::DoubleNotEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a != b) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a > b) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a >= b) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a < b) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
            return !isUnordered(a) && !isUnordered(b) && (a <= b) ? selectionA : selectionB;
        case MacroAssembler::DoubleEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a == b) ? selectionA : selectionB;
        case MacroAssembler::DoubleNotEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a != b) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a > b) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a >= b) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a < b) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrEqualOrUnordered:
            return isUnordered(a) || isUnordered(b) || (a <= b) ? selectionA : selectionB;
        } // switch
        RELEASE_ASSERT_NOT_REACHED();
    };

    for (auto a : operands) {
        for (auto b : operands) {
            arg1 = a;
            arg2 = b;
            CHECK_EQ(invoke<SelectionType>(testCode), expectedResult(a, b));
        }
    }
}

void testMoveConditionallyDouble2(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), destGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, destGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyDouble3(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionA = 0xbbad000a;
    unsigned corruptedSelectionB = 0xbbad000b;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        RELEASE_ASSERT(destGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(-1), destGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto aIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionAGPR, CCallHelpers::TrustedImm32(selectionA));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionA), destGPR);
        aIsUnchanged.link(&jit);

        auto bIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionBGPR, CCallHelpers::TrustedImm32(selectionB));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionB), destGPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyDouble3DestSameAsThenCase(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionB = 0xbbad000b;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = destGPR;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(destGPR == selectionAGPR);
        RELEASE_ASSERT(destGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto bIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionBGPR, CCallHelpers::TrustedImm32(selectionB));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionB), destGPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyDouble3DestSameAsElseCase(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionA = 0xbbad000a;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = destGPR;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        RELEASE_ASSERT(destGPR == selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto aIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionAGPR, CCallHelpers::TrustedImm32(selectionA));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionA), destGPR);
        aIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat2(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), GPRInfo::returnValueGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, destGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat3(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionA = 0xbbad000a;
    unsigned corruptedSelectionB = 0xbbad000b;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        RELEASE_ASSERT(destGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(-1), destGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto aIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionAGPR, CCallHelpers::TrustedImm32(selectionA));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionA), destGPR);
        aIsUnchanged.link(&jit);

        auto bIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionBGPR, CCallHelpers::TrustedImm32(selectionB));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionB), destGPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat3DestSameAsThenCase(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionB = 0xbbad000b;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = destGPR;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(destGPR == selectionAGPR);
        RELEASE_ASSERT(destGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto bIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionBGPR, CCallHelpers::TrustedImm32(selectionB));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionB), destGPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat3DestSameAsElseCase(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;
    unsigned corruptedSelectionA = 0xbbad000a;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg destGPR = GPRInfo::returnValueGPR;
        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = destGPR;
        RELEASE_ASSERT(destGPR != selectionAGPR);
        RELEASE_ASSERT(destGPR == selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT1);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1, selectionAGPR, selectionBGPR, destGPR);

        auto aIsUnchanged = jit.branch32(CCallHelpers::Equal, selectionAGPR, CCallHelpers::TrustedImm32(selectionA));
        jit.move(CCallHelpers::TrustedImm32(corruptedSelectionA), destGPR);
        aIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyDouble(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionA = 55555;
    double corruptedSelectionB = 66666;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = FPRInfo::fpRegT1;
        FPRReg selectionBFPR = FPRInfo::fpRegT2;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR != selectionAFPR);
        RELEASE_ASSERT(destFPR != selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyDouble(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), tempFPR);
        auto aIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionAFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionA), destFPR);
        aIsUnchanged.link(&jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), tempFPR);
        auto bIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionBFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionB), destFPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyDoubleDestSameAsThenCase(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionB = 66666;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = destFPR;
        FPRReg selectionBFPR = FPRInfo::fpRegT2;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR == selectionAFPR);
        RELEASE_ASSERT(destFPR != selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyDouble(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), tempFPR);
        auto bIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionBFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionB), destFPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyDoubleDestSameAsElseCase(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionA = 55555;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = FPRInfo::fpRegT1;
        FPRReg selectionBFPR = destFPR;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR != selectionAFPR);
        RELEASE_ASSERT(destFPR == selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyDouble(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), tempFPR);
        auto aIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionAFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionA), destFPR);
        aIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, doubleOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyFloat(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionA = 55555;
    double corruptedSelectionB = 66666;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = FPRInfo::fpRegT1;
        FPRReg selectionBFPR = FPRInfo::fpRegT2;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR != selectionAFPR);
        RELEASE_ASSERT(destFPR != selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyFloat(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), tempFPR);
        auto aIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionAFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionA), destFPR);
        aIsUnchanged.link(&jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), tempFPR);
        auto bIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionBFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionB), destFPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyFloatDestSameAsThenCase(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionB = 66666;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = destFPR;
        FPRReg selectionBFPR = FPRInfo::fpRegT2;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR == selectionAFPR);
        RELEASE_ASSERT(destFPR != selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyFloat(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), tempFPR);
        auto bIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionBFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionB), destFPR);
        bIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyFloatDestSameAsElseCase(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    float arg2 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;
    double corruptedSelectionA = 55555;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        FPRReg destFPR = FPRInfo::returnValueFPR;
        FPRReg selectionAFPR = FPRInfo::fpRegT1;
        FPRReg selectionBFPR = destFPR;
        FPRReg arg1FPR = FPRInfo::fpRegT3;
        FPRReg arg2FPR = FPRInfo::fpRegT4;

        RELEASE_ASSERT(destFPR != selectionAFPR);
        RELEASE_ASSERT(destFPR == selectionBFPR);
        RELEASE_ASSERT(destFPR != arg1FPR);
        RELEASE_ASSERT(destFPR != arg2FPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), arg1FPR);
        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg2), arg2FPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), selectionAFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), selectionBFPR);
        jit.moveDoubleConditionallyFloat(condition, arg1FPR, arg2FPR, selectionAFPR, selectionBFPR, destFPR);

        FPRReg tempFPR = FPRInfo::fpRegT5;
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), tempFPR);
        auto aIsUnchanged = jit.branchDouble(CCallHelpers::DoubleEqualAndOrdered, selectionAFPR, tempFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&corruptedSelectionA), destFPR);
        aIsUnchanged.link(&jit);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPoint(condition, testCode, arg1, arg2, floatOperands(), selectionA, selectionB);
}

template<typename T, typename SelectionType>
void testMoveConditionallyFloatingPointSameArg(MacroAssembler::DoubleCondition condition, const MacroAssemblerCodeRef<JSEntryPtrTag>& testCode, T& arg1, const Vector<T> operands, SelectionType selectionA, SelectionType selectionB)
{
    auto expectedResult = [&, condition] (T a) -> SelectionType {
        auto isUnordered = [] (double x) {
            return x != x;
        };
        switch (condition) {
        case MacroAssembler::DoubleEqualAndOrdered:
            return !isUnordered(a) && (a == a) ? selectionA : selectionB;
        case MacroAssembler::DoubleNotEqualAndOrdered:
            return !isUnordered(a) && (a != a) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanAndOrdered:
            return !isUnordered(a) && (a > a) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
            return !isUnordered(a) && (a >= a) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanAndOrdered:
            return !isUnordered(a) && (a < a) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
            return !isUnordered(a) && (a <= a) ? selectionA : selectionB;
        case MacroAssembler::DoubleEqualOrUnordered:
            return isUnordered(a) || (a == a) ? selectionA : selectionB;
        case MacroAssembler::DoubleNotEqualOrUnordered:
            return isUnordered(a) || (a != a) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrUnordered:
            return isUnordered(a) || (a > a) ? selectionA : selectionB;
        case MacroAssembler::DoubleGreaterThanOrEqualOrUnordered:
            return isUnordered(a) || (a >= a) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrUnordered:
            return isUnordered(a) || (a < a) ? selectionA : selectionB;
        case MacroAssembler::DoubleLessThanOrEqualOrUnordered:
            return isUnordered(a) || (a <= a) ? selectionA : selectionB;
        } // switch
        RELEASE_ASSERT_NOT_REACHED();
    };

    for (auto a : operands) {
        arg1 = a;
        CHECK_EQ(invoke<SelectionType>(testCode), expectedResult(a));
    }
}

void testMoveConditionallyDouble2SameArg(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), GPRInfo::returnValueGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, selectionAGPR, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyDouble3SameArg(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionAGPR);
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(-1), GPRInfo::returnValueGPR);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.moveConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, selectionAGPR, selectionBGPR, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, doubleOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat2SameArg(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), GPRInfo::returnValueGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, selectionAGPR, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, floatOperands(), selectionA, selectionB);
}

void testMoveConditionallyFloat3SameArg(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    unsigned selectionA = 42;
    unsigned selectionB = 17;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        GPRReg selectionAGPR = GPRInfo::argumentGPR2;
        GPRReg selectionBGPR = GPRInfo::argumentGPR3;
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionAGPR);
        RELEASE_ASSERT(GPRInfo::returnValueGPR != selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionA), selectionAGPR);
        jit.move(CCallHelpers::TrustedImm32(selectionB), selectionBGPR);
        jit.move(CCallHelpers::TrustedImm32(-1), GPRInfo::returnValueGPR);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.moveConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, selectionAGPR, selectionBGPR, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, floatOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyDoubleSameArg(MacroAssembler::DoubleCondition condition)
{
    double arg1 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), FPRInfo::fpRegT2);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), FPRInfo::fpRegT3);
        jit.moveDoubleConditionallyDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, FPRInfo::fpRegT2, FPRInfo::fpRegT3, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, doubleOperands(), selectionA, selectionB);
}

void testMoveDoubleConditionallyFloatSameArg(MacroAssembler::DoubleCondition condition)
{
    float arg1 = 0;
    double selectionA = 42.0;
    double selectionB = 17.0;

    auto testCode = compile([&, condition] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.loadFloat(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT0);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionA), FPRInfo::fpRegT2);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&selectionB), FPRInfo::fpRegT3);
        jit.moveDoubleConditionallyFloat(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT0, FPRInfo::fpRegT2, FPRInfo::fpRegT3, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    testMoveConditionallyFloatingPointSameArg(condition, testCode, arg1, floatOperands(), selectionA, selectionB);
}

#endif // CPU(X86_64) || CPU(ARM64)

#if ENABLE(MASM_PROBE)
void testProbeReadsArgumentRegisters()
{
    bool probeWasCalled = false;
    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.pushPair(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
        jit.pushPair(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3);

        jit.move(CCallHelpers::TrustedImm32(testWord32(0)), GPRInfo::argumentGPR0);
        jit.convertInt32ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT0);
        jit.move(CCallHelpers::TrustedImm32(testWord32(1)), GPRInfo::argumentGPR0);
        jit.convertInt32ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT1);
#if USE(JSVALUE64)
        jit.move(CCallHelpers::TrustedImm64(testWord(0)), GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm64(testWord(1)), GPRInfo::argumentGPR1);
        jit.move(CCallHelpers::TrustedImm64(testWord(2)), GPRInfo::argumentGPR2);
        jit.move(CCallHelpers::TrustedImm64(testWord(3)), GPRInfo::argumentGPR3);
#else
        jit.move(CCallHelpers::TrustedImm32(testWord(0)), GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm32(testWord(1)), GPRInfo::argumentGPR1);
        jit.move(CCallHelpers::TrustedImm32(testWord(2)), GPRInfo::argumentGPR2);
        jit.move(CCallHelpers::TrustedImm32(testWord(3)), GPRInfo::argumentGPR3);
#endif

        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeWasCalled = true;
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR0), testWord(0));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR1), testWord(1));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR2), testWord(2));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR3), testWord(3));

            CHECK_EQ(cpu.fpr(FPRInfo::fpRegT0), testWord32(0));
            CHECK_EQ(cpu.fpr(FPRInfo::fpRegT1), testWord32(1));
        });

        jit.popPair(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3);
        jit.popPair(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);

        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(probeWasCalled, true);
}

void testProbeWritesArgumentRegisters()
{
    // This test relies on testProbeReadsArgumentRegisters() having already validated
    // that we can read from argument registers. We'll use that ability to validate
    // that our writes did take effect.
    unsigned probeCallCount = 0;
    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.pushPair(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
        jit.pushPair(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3);

        // Pre-initialize with non-expected values.
#if USE(JSVALUE64)
        jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::argumentGPR1);
        jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::argumentGPR2);
        jit.move(CCallHelpers::TrustedImm64(0), GPRInfo::argumentGPR3);
#else
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR0);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR1);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR2);
        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::argumentGPR3);
#endif
        jit.convertInt32ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT0);
        jit.convertInt32ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT1);

        // Write expected values.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            cpu.gpr(GPRInfo::argumentGPR0) = testWord(0);
            cpu.gpr(GPRInfo::argumentGPR1) = testWord(1);
            cpu.gpr(GPRInfo::argumentGPR2) = testWord(2);
            cpu.gpr(GPRInfo::argumentGPR3) = testWord(3);
            
            cpu.fpr(FPRInfo::fpRegT0) = bitwise_cast<double>(testWord64(0));
            cpu.fpr(FPRInfo::fpRegT1) = bitwise_cast<double>(testWord64(1));
        });

        // Validate that expected values were written.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR0), testWord(0));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR1), testWord(1));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR2), testWord(2));
            CHECK_EQ(cpu.gpr(GPRInfo::argumentGPR3), testWord(3));

            CHECK_EQ(cpu.fpr<uint64_t>(FPRInfo::fpRegT0), testWord64(0));
            CHECK_EQ(cpu.fpr<uint64_t>(FPRInfo::fpRegT1), testWord64(1));
        });

        jit.popPair(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3);
        jit.popPair(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);

        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(probeCallCount, 2);
}

static NEVER_INLINE NOT_TAIL_CALLED int testFunctionToTrashGPRs(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{
    if (j > 0)
        return testFunctionToTrashGPRs(a + 1, b + a, c + b, d + 5, e - a, f * 1.5, g ^ a, h - b, i, j - 1);
    return a + 1;
}
static NEVER_INLINE NOT_TAIL_CALLED double testFunctionToTrashFPRs(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j)
{
    if (j > 0)
        return testFunctionToTrashFPRs(a + 1, b + a, c + b, d + 5, e - a, f * 1.5, pow(g, a), h - b, i, j - 1);
    return a + 1;
}

void testProbePreservesGPRS()
{
    // This test relies on testProbeReadsArgumentRegisters() and testProbeWritesArgumentRegisters()
    // having already validated that we can read and write from registers. We'll use these abilities
    // to validate that the probe preserves register values.
    unsigned probeCallCount = 0;
    CPUState originalState;

    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        // Write expected values into the registers (except for sp, fp, and pc).
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                originalState.gpr(id) = cpu.gpr(id);
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = testWord(static_cast<int>(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
                originalState.fpr(id) = cpu.fpr(id);
                cpu.fpr(id) = bitwise_cast<double>(testWord64(id));
            }
        });

        // Invoke the probe to call a lot of functions and trash register values.
        jit.probe([&] (Probe::Context&) {
            probeCallCount++;
            CHECK_EQ(testFunctionToTrashGPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 10);
            CHECK_EQ(testFunctionToTrashFPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 10);
        });

        // Validate that the registers have the expected values.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSP(id) || isFP(id)) {
                    CHECK_EQ(cpu.gpr(id), originalState.gpr(id));
                    continue;
                }
                if (isSpecialGPR(id))
                    continue;
                CHECK_EQ(cpu.gpr(id), testWord(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), testWord64(id));
        });

        // Restore the original state.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                cpu.fpr(id) = originalState.fpr(id);
        });

        // Validate that the original state was restored.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                CHECK_EQ(cpu.gpr(id), originalState.gpr(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), originalState.fpr<uint64_t>(id));
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(probeCallCount, 5);
}

void testProbeModifiesStackPointer(WTF::Function<void*(Probe::Context&)> computeModifiedStackPointer)
{
    unsigned probeCallCount = 0;
    CPUState originalState;
    void* originalSP { nullptr };
    void* modifiedSP { nullptr };
#if !(CPU(MIPS))
    uintptr_t modifiedFlags { 0 };
#endif
    
#if CPU(X86) || CPU(X86_64)
    auto flagsSPR = X86Registers::eflags;
    uintptr_t flagsMask = 0xc5;
#elif CPU(ARM_THUMB2)
    auto flagsSPR = ARMRegisters::apsr;
    uintptr_t flagsMask = 0xf8000000;
#elif CPU(ARM64)
    auto flagsSPR = ARM64Registers::nzcv;
    uintptr_t flagsMask = 0xf0000000;
#endif

    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        // Preserve original stack pointer and modify the sp, and
        // write expected values into other registers (except for fp, and pc).
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                originalState.gpr(id) = cpu.gpr(id);
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = testWord(static_cast<int>(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
                originalState.fpr(id) = cpu.fpr(id);
                cpu.fpr(id) = bitwise_cast<double>(testWord64(id));
            }

#if !(CPU(MIPS))
            originalState.spr(flagsSPR) = cpu.spr(flagsSPR);
            modifiedFlags = originalState.spr(flagsSPR) ^ flagsMask;
            cpu.spr(flagsSPR) = modifiedFlags;
#endif

            originalSP = cpu.sp();
            modifiedSP = computeModifiedStackPointer(context);
            cpu.sp() = modifiedSP;
        });

        // Validate that the registers have the expected values.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isFP(id)) {
                    CHECK_EQ(cpu.gpr(id), originalState.gpr(id));
                    continue;
                }
                if (isSpecialGPR(id))
                    continue;
                CHECK_EQ(cpu.gpr(id), testWord(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), testWord64(id));
#if !(CPU(MIPS))
            CHECK_EQ(cpu.spr(flagsSPR) & flagsMask, modifiedFlags & flagsMask);
#endif
            CHECK_EQ(cpu.sp(), modifiedSP);
        });

        // Restore the original state.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                cpu.fpr(id) = originalState.fpr(id);
#if !(CPU(MIPS))
            cpu.spr(flagsSPR) = originalState.spr(flagsSPR);
#endif
            cpu.sp() = originalSP;
        });

        // Validate that the original state was restored.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                CHECK_EQ(cpu.gpr(id), originalState.gpr(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), originalState.fpr<uint64_t>(id));
#if !(CPU(MIPS))
            CHECK_EQ(cpu.spr(flagsSPR) & flagsMask, originalState.spr(flagsSPR) & flagsMask);
#endif
            CHECK_EQ(cpu.sp(), originalSP);
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(probeCallCount, 4);
}

void testProbeModifiesStackPointerToInsideProbeStateOnStack()
{
    size_t increment = sizeof(uintptr_t);
#if CPU(ARM64)
    // The ARM64 probe uses ldp and stp which require 16 byte alignment.
    increment = 2 * sizeof(uintptr_t);
#endif
    for (size_t offset = 0; offset < sizeof(Probe::State); offset += increment) {
        testProbeModifiesStackPointer([=] (Probe::Context& context) -> void* {
            return reinterpret_cast<uint8_t*>(probeStateForContext(context)) + offset;

        });
    }
}

void testProbeModifiesStackPointerToNBytesBelowSP()
{
    size_t increment = sizeof(uintptr_t);
#if CPU(ARM64)
    // The ARM64 probe uses ldp and stp which require 16 byte alignment.
    increment = 2 * sizeof(uintptr_t);
#endif
    for (size_t offset = 0; offset < 1 * KB; offset += increment) {
        testProbeModifiesStackPointer([=] (Probe::Context& context) -> void* {
            return context.cpu.sp<uint8_t*>() - offset;
        });
    }
}

void testProbeModifiesProgramCounter()
{
    // This test relies on testProbeReadsArgumentRegisters() and testProbeWritesArgumentRegisters()
    // having already validated that we can read and write from registers. We'll use these abilities
    // to validate that the probe preserves register values.
    unsigned probeCallCount = 0;
    bool continuationWasReached = false;

    MacroAssemblerCodeRef<JSEntryPtrTag> continuation = compile([&] (CCallHelpers& jit) {
        // Validate that we reached the continuation.
        jit.probe([&] (Probe::Context&) {
            probeCallCount++;
            continuationWasReached = true;
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        // Write expected values into the registers.
        jit.probe([&] (Probe::Context& context) {
            probeCallCount++;
            context.cpu.pc() = untagCodePtr(continuation.code().executableAddress(), JSEntryPtrTag);
        });

        jit.breakpoint(); // We should never get here.
    });
    CHECK_EQ(probeCallCount, 2);
    CHECK_EQ(continuationWasReached, true);
}

void testProbeModifiesStackValues()
{
    unsigned probeCallCount = 0;
    CPUState originalState;
    void* originalSP { nullptr };
    void* newSP { nullptr };
#if !CPU(MIPS)
    uintptr_t modifiedFlags { 0 };
#endif
    size_t numberOfExtraEntriesToWrite { 10 }; // ARM64 requires that this be 2 word aligned.

#if CPU(X86) || CPU(X86_64)
    MacroAssembler::SPRegisterID flagsSPR = X86Registers::eflags;
    uintptr_t flagsMask = 0xc5;
#elif CPU(ARM_THUMB2)
    MacroAssembler::SPRegisterID flagsSPR = ARMRegisters::apsr;
    uintptr_t flagsMask = 0xf8000000;
#elif CPU(ARM64)
    MacroAssembler::SPRegisterID flagsSPR = ARM64Registers::nzcv;
    uintptr_t flagsMask = 0xf0000000;
#endif

    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        // Write expected values into the registers.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            auto& stack = context.stack();
            probeCallCount++;

            // Preserve the original CPU state.
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                originalState.gpr(id) = cpu.gpr(id);
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = testWord(static_cast<int>(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
                originalState.fpr(id) = cpu.fpr(id);
                cpu.fpr(id) = bitwise_cast<double>(testWord64(id));
            }
#if !(CPU(MIPS))
            originalState.spr(flagsSPR) = cpu.spr(flagsSPR);
            modifiedFlags = originalState.spr(flagsSPR) ^ flagsMask;
            cpu.spr(flagsSPR) = modifiedFlags;
#endif

            // Ensure that we'll be writing over the regions of the stack where the Probe::State is.
            originalSP = cpu.sp();
            newSP = reinterpret_cast<uintptr_t*>(probeStateForContext(context)) - numberOfExtraEntriesToWrite;
            cpu.sp() = newSP;

            // Fill the stack with values.
            uintptr_t* p = reinterpret_cast<uintptr_t*>(newSP);
            int count = 0;
            stack.set<double>(p++, 1.234567);
            if (is32Bit())
                p++; // On 32-bit targets, a double takes up 2 uintptr_t.
            while (p < reinterpret_cast<uintptr_t*>(originalSP))
                stack.set<uintptr_t>(p++, testWord(count++));
        });

        // Validate that the registers and stack have the expected values.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            auto& stack = context.stack();
            probeCallCount++;

            // Validate the register values.
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isFP(id)) {
                    CHECK_EQ(cpu.gpr(id), originalState.gpr(id));
                    continue;
                }
                if (isSpecialGPR(id))
                    continue;
                CHECK_EQ(cpu.gpr(id), testWord(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), testWord64(id));
#if !(CPU(MIPS))
            CHECK_EQ(cpu.spr(flagsSPR) & flagsMask, modifiedFlags & flagsMask);
#endif
            CHECK_EQ(cpu.sp(), newSP);

            // Validate the stack values.
            uintptr_t* p = reinterpret_cast<uintptr_t*>(newSP);
            int count = 0;
            CHECK_EQ(stack.get<double>(p++), 1.234567);
            if (is32Bit())
                p++; // On 32-bit targets, a double takes up 2 uintptr_t.
            while (p < reinterpret_cast<uintptr_t*>(originalSP))
                CHECK_EQ(stack.get<uintptr_t>(p++), testWord(count++));
        });

        // Restore the original state.
        jit.probe([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                cpu.fpr(id) = originalState.fpr(id);
#if !(CPU(MIPS))
            cpu.spr(flagsSPR) = originalState.spr(flagsSPR);
#endif
            cpu.sp() = originalSP;
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    CHECK_EQ(probeCallCount, 3);
}
#endif // ENABLE(MASM_PROBE)

void testOrImmMem()
{
    // FIXME: this does not test that the or does not touch beyond its width.
    // I am not sure how to do such a test without a lot of complexity (running multiple threads, with a race on the high bits of the memory location).
    uint64_t memoryLocation = 0x12341234;
    auto or32 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.or32(CCallHelpers::TrustedImm32(42), CCallHelpers::AbsoluteAddress(&memoryLocation));
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    invoke<void>(or32);
    CHECK_EQ(memoryLocation, 0x12341234 | 42);

    memoryLocation = 0x12341234;
    auto or16 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.or16(CCallHelpers::TrustedImm32(42), CCallHelpers::AbsoluteAddress(&memoryLocation));
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    invoke<void>(or16);
    CHECK_EQ(memoryLocation, 0x12341234 | 42);

    memoryLocation = 0x12341234;
    auto or16InvalidLogicalImmInARM64 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.or16(CCallHelpers::TrustedImm32(0), CCallHelpers::AbsoluteAddress(&memoryLocation));
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    invoke<void>(or16InvalidLogicalImmInARM64);
    CHECK_EQ(memoryLocation, 0x12341234);
}

void testByteSwap()
{
#if CPU(X86_64) || CPU(ARM64)
    auto byteSwap16 = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        jit.byteSwap16(GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(invoke<uint64_t>(byteSwap16, 0xaabbccddee001122), static_cast<uint64_t>(0x2211));
    CHECK_EQ(invoke<uint64_t>(byteSwap16, 0xaabbccddee00ffaa), static_cast<uint64_t>(0xaaff));

    auto byteSwap32 = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        jit.byteSwap32(GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(invoke<uint64_t>(byteSwap32, 0xaabbccddee001122), static_cast<uint64_t>(0x221100ee));
    CHECK_EQ(invoke<uint64_t>(byteSwap32, 0xaabbccddee00ffaa), static_cast<uint64_t>(0xaaff00ee));

    auto byteSwap64 = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        jit.byteSwap64(GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    CHECK_EQ(invoke<uint64_t>(byteSwap64, 0xaabbccddee001122), static_cast<uint64_t>(0x221100eeddccbbaa));
    CHECK_EQ(invoke<uint64_t>(byteSwap64, 0xaabbccddee00ffaa), static_cast<uint64_t>(0xaaff00eeddccbbaa));
#endif
}

void testMoveDoubleConditionally32()
{
#if CPU(X86_64) | CPU(ARM64)
    double arg1 = 0;
    double arg2 = 0;
    const double zero = -0;

    const double chosenDouble = 6.00000059604644775390625;
    CHECK_EQ(static_cast<double>(static_cast<float>(chosenDouble)) == chosenDouble, false);

    auto sel = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&zero), FPRInfo::returnValueFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT1);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT2);

        jit.move(MacroAssembler::TrustedImm32(-1), GPRInfo::regT0);
        jit.moveDoubleConditionally32(MacroAssembler::Equal, GPRInfo::regT0, GPRInfo::regT0, FPRInfo::fpRegT1, FPRInfo::fpRegT2, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    arg1 = chosenDouble;
    arg2 = 43;
    CHECK_EQ(invoke<double>(sel), chosenDouble);

    arg1 = 43;
    arg2 = chosenDouble;
    CHECK_EQ(invoke<double>(sel), 43.0);

#endif
}

void testMoveDoubleConditionally64()
{
#if CPU(X86_64) | CPU(ARM64)
    double arg1 = 0;
    double arg2 = 0;
    const double zero = -0;

    const double chosenDouble = 6.00000059604644775390625;
    CHECK_EQ(static_cast<double>(static_cast<float>(chosenDouble)) == chosenDouble, false);

    auto sel = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&zero), FPRInfo::returnValueFPR);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT1);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT2);

        jit.move(MacroAssembler::TrustedImm64(-1), GPRInfo::regT0);
        jit.moveDoubleConditionally64(MacroAssembler::Equal, GPRInfo::regT0, GPRInfo::regT0, FPRInfo::fpRegT1, FPRInfo::fpRegT2, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    arg1 = chosenDouble;
    arg2 = 43;
    CHECK_EQ(invoke<double>(sel), chosenDouble);

    arg1 = 43;
    arg2 = chosenDouble;
    CHECK_EQ(invoke<double>(sel), 43.0);

#endif
}

static void testCagePreservesPACFailureBit()
{
#if GIGACAGE_ENABLED
    // Placate ASan builds and any environments that disables the Gigacage.
    if (!Gigacage::shouldBeEnabled())
        return;

    RELEASE_ASSERT(!Gigacage::disablingPrimitiveGigacageIsForbidden());
    auto cage = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.cageConditionally(Gigacage::Primitive, GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });

    void* ptr = Gigacage::tryMalloc(Gigacage::Primitive, 1);
    void* taggedPtr = tagArrayPtr(ptr, 1);
    RELEASE_ASSERT(hasOneBitSet(Gigacage::size(Gigacage::Primitive) << 2));
    void* notCagedPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + (Gigacage::size(Gigacage::Primitive) << 2));
    CHECK_NOT_EQ(Gigacage::caged(Gigacage::Primitive, notCagedPtr), notCagedPtr);
    void* taggedNotCagedPtr = tagArrayPtr(notCagedPtr, 1);

    if (isARM64E()) {
        CHECK_NOT_EQ(invoke<void*>(cage, taggedPtr, 2), ptr);
        CHECK_NOT_EQ(invoke<void*>(cage, taggedNotCagedPtr, 1), ptr);
        void* cagedTaggedNotCagedPtr = invoke<void*>(cage, taggedNotCagedPtr, 1);
        CHECK_NOT_EQ(cagedTaggedNotCagedPtr, removeArrayPtrTag(cagedTaggedNotCagedPtr));
    } else
        CHECK_EQ(invoke<void*>(cage, taggedPtr, 2), ptr);

    CHECK_EQ(invoke<void*>(cage, taggedPtr, 1), ptr);

    auto cageWithoutAuthentication = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.cageWithoutUntagging(Gigacage::Primitive, GPRInfo::argumentGPR0);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });

    CHECK_EQ(invoke<void*>(cageWithoutAuthentication, taggedPtr), taggedPtr);
    if (isARM64E()) {
        CHECK_NOT_EQ(invoke<void*>(cageWithoutAuthentication, taggedNotCagedPtr), taggedNotCagedPtr);
        CHECK_NOT_EQ(invoke<void*>(cageWithoutAuthentication, taggedNotCagedPtr), tagArrayPtr(notCagedPtr, 1));
        CHECK_NOT_EQ(invoke<void*>(cageWithoutAuthentication, taggedNotCagedPtr), taggedPtr);
        CHECK_NOT_EQ(invoke<void*>(cageWithoutAuthentication, taggedNotCagedPtr), tagArrayPtr(ptr, 1));
    }

    Gigacage::free(Gigacage::Primitive, ptr);
#endif
}

static void testBranchIfType()
{
    using JSC::JSType;
    struct CellLike {
        uint32_t structureID;
        uint8_t indexingType;
        JSType type;
    };
    CHECK_EQ(JSCell::typeInfoTypeOffset(), OBJECT_OFFSETOF(CellLike, type));

    auto isType = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        auto isType = jit.branchIfType(GPRInfo::argumentGPR0, JSC::JSTypeRange { JSType(FirstTypedArrayType), JSType(LastTypedArrayTypeExcludingDataView) });
        jit.move(CCallHelpers::TrustedImm32(false), GPRInfo::returnValueGPR);
        auto done = jit.jump();
        isType.link(&jit);
        jit.move(CCallHelpers::TrustedImm32(true), GPRInfo::returnValueGPR);
        done.link(&jit);
        emitFunctionEpilogue(jit);
        jit.ret();
    });

    CellLike cell;
    for (unsigned i = JSC::FirstTypedArrayType; i <= JSC::LastTypedArrayTypeExcludingDataView; ++i) {
        cell.type = JSType(i);
        CHECK_EQ(invoke<bool>(isType, &cell), true);
    }

    cell.type = JSType(LastTypedArrayType);
    CHECK_EQ(invoke<bool>(isType, &cell), false);
    cell.type = JSType(FirstTypedArrayType - 1);
    CHECK_EQ(invoke<bool>(isType, &cell), false);
}

static void testBranchIfNotType()
{
    using JSC::JSType;
    struct CellLike {
        uint32_t structureID;
        uint8_t indexingType;
        JSType type;
    };
    CHECK_EQ(JSCell::typeInfoTypeOffset(), OBJECT_OFFSETOF(CellLike, type));

    auto isNotType = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        auto isNotType = jit.branchIfNotType(GPRInfo::argumentGPR0, JSC::JSTypeRange { JSType(FirstTypedArrayType), JSType(LastTypedArrayTypeExcludingDataView) });
        jit.move(CCallHelpers::TrustedImm32(false), GPRInfo::returnValueGPR);
        auto done = jit.jump();
        isNotType.link(&jit);
        jit.move(CCallHelpers::TrustedImm32(true), GPRInfo::returnValueGPR);
        done.link(&jit);
        emitFunctionEpilogue(jit);
        jit.ret();
    });

    CellLike cell;
    for (unsigned i = JSC::FirstTypedArrayType; i <= JSC::LastTypedArrayTypeExcludingDataView; ++i) {
        cell.type = JSType(i);
        CHECK_EQ(invoke<bool>(isNotType, &cell), false);
    }

    cell.type = JSType(LastTypedArrayType);
    CHECK_EQ(invoke<bool>(isNotType, &cell), true);
    cell.type = JSType(FirstTypedArrayType - 1);
    CHECK_EQ(invoke<bool>(isNotType, &cell), true);
}

#define RUN(test) do {                          \
        if (!shouldRun(#test))                  \
            break;                              \
        numberOfTests++;                        \
        tasks.append(                           \
            createSharedTask<void()>(           \
                [&] () {                        \
                    dataLog(#test "...\n");     \
                    test;                       \
                    dataLog(#test ": OK!\n");   \
                }));                            \
    } while (false);

void run(const char* filter)
{
    JSC::initializeThreading();
    unsigned numberOfTests = 0;

    Deque<RefPtr<SharedTask<void()>>> tasks;

    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || WTF::findIgnoringASCIICaseWithoutLength(testName, filter) != WTF::notFound;
    };

    RUN(testSimple());
    RUN(testGetEffectiveAddress(0xff00, 42, 8, CCallHelpers::TimesEight));
    RUN(testGetEffectiveAddress(0xff00, -200, -300, CCallHelpers::TimesEight));
    RUN(testBranchTruncateDoubleToInt32(0, 0));
    RUN(testBranchTruncateDoubleToInt32(42, 42));
    RUN(testBranchTruncateDoubleToInt32(42.7, 42));
    RUN(testBranchTruncateDoubleToInt32(-1234, -1234));
    RUN(testBranchTruncateDoubleToInt32(-1234.56, -1234));
    RUN(testBranchTruncateDoubleToInt32(std::numeric_limits<double>::infinity(), 0));
    RUN(testBranchTruncateDoubleToInt32(-std::numeric_limits<double>::infinity(), 0));
    RUN(testBranchTruncateDoubleToInt32(std::numeric_limits<double>::quiet_NaN(), 0));
    RUN(testBranchTruncateDoubleToInt32(std::numeric_limits<double>::signaling_NaN(), 0));
    RUN(testBranchTruncateDoubleToInt32(std::numeric_limits<double>::max(), 0));
    RUN(testBranchTruncateDoubleToInt32(-std::numeric_limits<double>::max(), 0));
    // We run this last one to make sure that we don't use flags that were not
    // reset to check a conversion result
    RUN(testBranchTruncateDoubleToInt32(123, 123));

#define FOR_EACH_DOUBLE_CONDITION_RUN(__test) \
    do { \
        RUN(__test(MacroAssembler::DoubleEqualAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleNotEqualAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleGreaterThanAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleGreaterThanOrEqualAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleLessThanAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleLessThanOrEqualAndOrdered)); \
        RUN(__test(MacroAssembler::DoubleEqualOrUnordered)); \
        RUN(__test(MacroAssembler::DoubleNotEqualOrUnordered)); \
        RUN(__test(MacroAssembler::DoubleGreaterThanOrUnordered)); \
        RUN(__test(MacroAssembler::DoubleGreaterThanOrEqualOrUnordered)); \
        RUN(__test(MacroAssembler::DoubleLessThanOrUnordered)); \
        RUN(__test(MacroAssembler::DoubleLessThanOrEqualOrUnordered)); \
    } while (false)

    FOR_EACH_DOUBLE_CONDITION_RUN(testCompareDouble);
    FOR_EACH_DOUBLE_CONDITION_RUN(testCompareDoubleSameArg);

    RUN(testMul32WithImmediates());

#if CPU(X86_64)
    RUN(testBranchTestBit32RegReg());
    RUN(testBranchTestBit32RegImm());
    RUN(testBranchTestBit32AddrImm());
    RUN(testBranchTestBit64RegReg());
    RUN(testBranchTestBit64RegImm());
    RUN(testBranchTestBit64AddrImm());
#endif

#if CPU(X86_64) || CPU(ARM64)
    RUN(testClearBit64());
    RUN(testClearBits64WithMask());
    RUN(testClearBits64WithMaskTernary());
    RUN(testCountTrailingZeros64());
    RUN(testCountTrailingZeros64WithoutNullCheck());
    RUN(testStore64Imm64AddressPointer());
#endif

#if CPU(ARM64)
    RUN(testMul32SignExtend());
#endif

#if CPU(X86) || CPU(X86_64) || CPU(ARM64)
    FOR_EACH_DOUBLE_CONDITION_RUN(testCompareFloat);
#endif

#if CPU(X86_64) || CPU(ARM64)
    // Comparing 2 different registers.
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble2);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble3);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble3DestSameAsThenCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble3DestSameAsElseCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat2);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat3);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat3DestSameAsThenCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat3DestSameAsElseCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyDouble);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyDoubleDestSameAsThenCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyDoubleDestSameAsElseCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyFloat);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyFloatDestSameAsThenCase);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyFloatDestSameAsElseCase);

    // Comparing the same register against itself.
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble2SameArg);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyDouble3SameArg);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat2SameArg);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveConditionallyFloat3SameArg);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyDoubleSameArg);
    FOR_EACH_DOUBLE_CONDITION_RUN(testMoveDoubleConditionallyFloatSameArg);
#endif

#if ENABLE(MASM_PROBE)
    RUN(testProbeReadsArgumentRegisters());
    RUN(testProbeWritesArgumentRegisters());
    RUN(testProbePreservesGPRS());
    RUN(testProbeModifiesStackPointerToInsideProbeStateOnStack());
    RUN(testProbeModifiesStackPointerToNBytesBelowSP());
    RUN(testProbeModifiesProgramCounter());
    RUN(testProbeModifiesStackValues());
#endif // ENABLE(MASM_PROBE)

    RUN(testByteSwap());
    RUN(testMoveDoubleConditionally32());
    RUN(testMoveDoubleConditionally64());

    RUN(testCagePreservesPACFailureBit());

    RUN(testBranchIfType());
    RUN(testBranchIfNotType());

    RUN(testOrImmMem());

    if (tasks.isEmpty())
        usage();

    Lock lock;

    Vector<Ref<Thread>> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(
            Thread::create(
                "testmasm thread",
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

    for (auto& thread : threads)
        thread->waitForCompletion();
    crashLock.lock();
    dataLog("Completed ", numberOfTests, " tests\n");
}

} // anonymous namespace

#else // not ENABLE(JIT)

static void run(const char*)
{
    dataLog("JIT is not enabled.\n");
}

#endif // ENABLE(JIT)

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

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
