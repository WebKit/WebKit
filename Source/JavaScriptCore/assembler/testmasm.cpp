/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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


#if CPU(X86) || CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
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

namespace WTF {

static void printInternal(PrintStream& out, void* value)
{
    out.printf("%p", value);
}

} // namespace WTF

namespace JSC {
namespace Probe {

JS_EXPORT_PRIVATE void* probeStateForContext(Probe::Context&);

} // namespace Probe
} // namespace JSC

using namespace JSC;

namespace {

using CPUState = Probe::CPUState;

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
#elif CPU(RISCV64)
    if (id == RISCV64Registers::zero || id == RISCV64Registers::ra || id == RISCV64Registers::gp || id == RISCV64Registers::tp)
        return true;
#endif
    return false;
}

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
    void* executableAddress = untagCFunctionPtr<JSEntryPtrTag>(code.code().taggedPtr());
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(executableAddress);

#if CPU(RISCV64)
    // RV64 calling convention requires all 32-bit values to be sign-extended into the whole register.
    // JSC JIT is tailored for other ISAs that pass these values in 32-bit-wide registers, which RISC-V
    // doesn't support, so any 32-bit value passed in return-value registers has to be manually sign-extended.
    // This mirrors sign-extension of 32-bit values in argument registers on RV64 in CCallHelpers.h.
    if constexpr (std::is_integral_v<T>) {
        T returnValue = function(arguments...);
        if constexpr (sizeof(T) == 4) {
            asm volatile(
                "sext.w %[out_value], %[in_value]\n\t"
                : [out_value] "=r" (returnValue)
                : [in_value] "r" (returnValue));
        }
        return returnValue;
    }
#endif

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
    const uint64_t valAsUInt = bitwise_cast<uint64_t>(val);
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
                MacroAssembler::Address(MacroAssembler::stackPointerRegister));
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt & 0xffffffff),
                MacroAssembler::Address(MacroAssembler::stackPointerRegister, 4));
        } else {
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt & 0xffffffff),
                MacroAssembler::Address(MacroAssembler::stackPointerRegister));
            jit.store32(CCallHelpers::TrustedImm32(valAsUInt >> 32),
                MacroAssembler::Address(MacroAssembler::stackPointerRegister, 4));
        }
        jit.loadDouble(MacroAssembler::Address(MacroAssembler::stackPointerRegister), FPRInfo::fpRegT0);

        MacroAssembler::Jump done;
        done = jit.branchTruncateDoubleToInt32(FPRInfo::fpRegT0, GPRInfo::returnValueGPR, MacroAssembler::BranchIfTruncateSuccessful);

        jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);

        done.link(&jit);
        jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentBytes()), MacroAssembler::stackPointerRegister);
        emitFunctionEpilogue(jit);
        jit.ret();
    }), expected);
}

void testBranchTest8()
{
    for (auto value : int32Operands()) {
        for (auto value2 : int32Operands()) {
            auto test1 = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                auto branch = jit.branchTest8(MacroAssembler::NonZero, CCallHelpers::Address(GPRInfo::argumentGPR0, 1), CCallHelpers::TrustedImm32(value2));
                jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
                auto done = jit.jump();
                branch.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
                done.link(&jit);

                emitFunctionEpilogue(jit);
                jit.ret();
            });

            auto test2 = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                auto branch = jit.branchTest8(MacroAssembler::NonZero, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne), CCallHelpers::TrustedImm32(value2));
                jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
                auto done = jit.jump();
                branch.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
                done.link(&jit);

                emitFunctionEpilogue(jit);
                jit.ret();
            });

            int result = 0;
            if (static_cast<uint8_t>(value) & static_cast<uint8_t>(value2))
                result = 1;

            uint8_t array[] = {
                0,
                static_cast<uint8_t>(value)
            };
            CHECK_EQ(invoke<int>(test1, array), result);
            CHECK_EQ(invoke<int>(test2, array, 1), result);
        }
    }
}

void testBranchTest16()
{
    for (auto value : int32Operands()) {
        for (auto value2 : int32Operands()) {
            auto test1 = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                auto branch = jit.branchTest16(MacroAssembler::NonZero, CCallHelpers::Address(GPRInfo::argumentGPR0, 2), CCallHelpers::TrustedImm32(value2));
                jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
                auto done = jit.jump();
                branch.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
                done.link(&jit);

                emitFunctionEpilogue(jit);
                jit.ret();
            });

            auto test2 = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                auto branch = jit.branchTest16(MacroAssembler::NonZero, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo), CCallHelpers::TrustedImm32(value2));
                jit.move(CCallHelpers::TrustedImm32(0), GPRInfo::returnValueGPR);
                auto done = jit.jump();
                branch.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(1), GPRInfo::returnValueGPR);
                done.link(&jit);

                emitFunctionEpilogue(jit);
                jit.ret();
            });

            int result = 0;
            if (static_cast<uint16_t>(value) & static_cast<uint16_t>(value2))
                result = 1;

            uint16_t array[] = {
                0,
                static_cast<uint16_t>(value)
            };
            CHECK_EQ(invoke<int>(test1, array), result);
            CHECK_EQ(invoke<int>(test2, array, 1), result);
        }
    }
}

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

    uint64_t savedMask = 0;
    auto test2 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.probeDebug([&] (Probe::Context& context) {
            savedMask = context.gpr<uint64_t>(GPRInfo::argumentGPR1);
        });

        jit.clearBits64WithMask(GPRInfo::argumentGPR1, GPRInfo::argumentGPR0, CCallHelpers::ClearBitsAttributes::MustPreserveMask);

        jit.probeDebug([&] (Probe::Context& context) {
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

    uint64_t savedMask = 0;
    auto test2 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.move(GPRInfo::argumentGPR0, GPRInfo::argumentGPR2);
        jit.move(GPRInfo::argumentGPR1, GPRInfo::argumentGPR3);

        jit.probeDebug([&] (Probe::Context& context) {
            savedMask = context.gpr<uint64_t>(GPRInfo::argumentGPR2);
        });

        jit.clearBits64WithMask(GPRInfo::argumentGPR2, GPRInfo::argumentGPR3, GPRInfo::returnValueGPR, CCallHelpers::ClearBitsAttributes::MustPreserveMask);

        jit.probeDebug([&] (Probe::Context& context) {
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

void testShiftAndAdd()
{
    constexpr intptr_t basePointer = 0x1234abcd;

    enum class Reg {
        ArgumentGPR0,
        ArgumentGPR1,
        ArgumentGPR2,
        ArgumentGPR3,
        ScratchGPR
    };

    auto test = [&] (intptr_t index, uint8_t shift, Reg destReg, Reg baseReg, Reg indexReg) {
        auto test = compile([=] (CCallHelpers& jit) {
            CCallHelpers::RegisterID scratchGPR = jit.scratchRegister();

            auto registerIDForReg = [=] (Reg reg) -> CCallHelpers::RegisterID {
                switch (reg) {
                case Reg::ArgumentGPR0: return GPRInfo::argumentGPR0;
                case Reg::ArgumentGPR1: return GPRInfo::argumentGPR1;
                case Reg::ArgumentGPR2: return GPRInfo::argumentGPR2;
                case Reg::ArgumentGPR3: return GPRInfo::argumentGPR3;
                case Reg::ScratchGPR: return scratchGPR;
                }
                RELEASE_ASSERT_NOT_REACHED();
            };

            CCallHelpers::RegisterID destGPR = registerIDForReg(destReg);
            CCallHelpers::RegisterID baseGPR = registerIDForReg(baseReg);
            CCallHelpers::RegisterID indexGPR = registerIDForReg(indexReg);

            emitFunctionPrologue(jit);
            jit.pushPair(scratchGPR, GPRInfo::argumentGPR3);

            jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(basePointer)), baseGPR);
            jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(index)), indexGPR);
            jit.shiftAndAdd(baseGPR, indexGPR, shift, destGPR);

            jit.probeDebug([=] (Probe::Context& context) {
                if (baseReg != destReg)
                    CHECK_EQ(context.gpr<intptr_t>(baseGPR), basePointer);
                if (indexReg != destReg)
                    CHECK_EQ(context.gpr<intptr_t>(indexGPR), index);
            });
            jit.move(destGPR, GPRInfo::returnValueGPR);

            jit.popPair(scratchGPR, GPRInfo::argumentGPR3);
            emitFunctionEpilogue(jit);
            jit.ret();
        });

        CHECK_EQ(invoke<intptr_t>(test), basePointer + (index << shift));
    };

    for (auto index : int32Operands()) {
        for (uint8_t shift = 0; shift < 32; ++shift) {
            test(index, shift, Reg::ScratchGPR, Reg::ScratchGPR, Reg::ArgumentGPR3);     // Scenario: dest == base == scratchRegister.
            test(index, shift, Reg::ArgumentGPR2, Reg::ArgumentGPR2, Reg::ArgumentGPR3); // Scenario: dest == base != scratchRegister.
            test(index, shift, Reg::ScratchGPR, Reg::ArgumentGPR2, Reg::ScratchGPR);     // Scenario: dest == index == scratchRegister.
            test(index, shift, Reg::ArgumentGPR3, Reg::ArgumentGPR2, Reg::ArgumentGPR3); // Scenario: dest == index != scratchRegister.
            test(index, shift, Reg::ArgumentGPR1, Reg::ArgumentGPR2, Reg::ArgumentGPR3); // Scenario: all different registers, no scratchRegister.
            test(index, shift, Reg::ScratchGPR, Reg::ArgumentGPR2, Reg::ArgumentGPR3);   // Scenario: all different registers, dest == scratchRegister.
            test(index, shift, Reg::ArgumentGPR1, Reg::ScratchGPR, Reg::ArgumentGPR3);   // Scenario: all different registers, base == scratchRegister.
            test(index, shift, Reg::ArgumentGPR1, Reg::ArgumentGPR2, Reg::ScratchGPR);   // Scenario: all different registers, index == scratchRegister.
        }
    }
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
void testMultiplySignExtend32()
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

void testMultiplyZeroExtend32()
{
    for (auto nOperand : int32Operands()) {
        auto mul = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.multiplyZeroExtend32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto mOperand : int32Operands()) {
            uint32_t n = nOperand;
            uint32_t m = mOperand;
            CHECK_EQ(invoke<uint64_t>(mul, n, m), static_cast<uint64_t>(n) * static_cast<uint64_t>(m));
        }
    }
}

void testMultiplyAddSignExtend32()
{
    // d = SExt32(n) * SExt32(m) + a
    auto add = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplyAddSignExtend32(GPRInfo::argumentGPR0, 
            GPRInfo::argumentGPR1, 
            GPRInfo::argumentGPR2, 
            GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto a : int64Operands())
                CHECK_EQ(invoke<int64_t>(add, n, m, a), static_cast<int64_t>(n) * static_cast<int64_t>(m) + a);
        }
    }
}

void testMultiplyAddZeroExtend32()
{
    // d = ZExt32(n) * ZExt32(m) + a
    auto add = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplyAddZeroExtend32(GPRInfo::argumentGPR0, 
            GPRInfo::argumentGPR1, 
            GPRInfo::argumentGPR2, 
            GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto a : int64Operands()) {
                uint32_t un = n;
                uint32_t um = m;
                CHECK_EQ(invoke<int64_t>(add, n, m, a), static_cast<int64_t>(un) * static_cast<int64_t>(um) + a);
            }
        }
    }
}

void testSub32Args()
{
    for (auto value : int32Operands()) {
        auto sub = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.sub32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value2 : int32Operands())
            CHECK_EQ(invoke<uint32_t>(sub, value, value2), static_cast<uint32_t>(value - value2));
    }
}

void testSub32Imm()
{
    for (auto immediate : int32Operands()) {
        for (auto immediate2 : int32Operands()) {
            auto sub = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                jit.move(CCallHelpers::TrustedImm32(immediate), GPRInfo::returnValueGPR);
                jit.sub32(CCallHelpers::TrustedImm32(immediate2), GPRInfo::returnValueGPR);

                emitFunctionEpilogue(jit);
                jit.ret();
            });
            CHECK_EQ(invoke<uint32_t>(sub), static_cast<uint32_t>(immediate - immediate2));
        }
    }
}

void testSub64Imm32()
{
    for (auto immediate : int64Operands()) {
        for (auto immediate2 : int32Operands()) {
            auto sub = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                jit.move(CCallHelpers::TrustedImm64(immediate), GPRInfo::returnValueGPR);
                jit.sub64(CCallHelpers::TrustedImm32(immediate2), GPRInfo::returnValueGPR);

                emitFunctionEpilogue(jit);
                jit.ret();
            });
            CHECK_EQ(invoke<uint64_t>(sub), static_cast<uint64_t>(immediate - immediate2));
        }
    }
}

void testSub64ArgImm32()
{
    for (auto immediate : int32Operands()) {
        auto sub = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.sub64(GPRInfo::argumentGPR0, CCallHelpers::TrustedImm32(immediate), GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value : int64Operands())
            CHECK_EQ(invoke<int64_t>(sub, value), static_cast<int64_t>(value - immediate));
    }
}

void testSub64Imm64()
{
    for (auto immediate : int64Operands()) {
        for (auto immediate2 : int64Operands()) {
            auto sub = compile([=] (CCallHelpers& jit) {
                emitFunctionPrologue(jit);

                jit.move(CCallHelpers::TrustedImm64(immediate), GPRInfo::returnValueGPR);
                jit.sub64(CCallHelpers::TrustedImm64(immediate2), GPRInfo::returnValueGPR);

                emitFunctionEpilogue(jit);
                jit.ret();
            });
            CHECK_EQ(invoke<uint64_t>(sub), static_cast<uint64_t>(immediate - immediate2));
        }
    }
}

void testSub64ArgImm64()
{
    for (auto immediate : int64Operands()) {
        auto sub = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.sub64(GPRInfo::argumentGPR0, CCallHelpers::TrustedImm64(immediate), GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value : int64Operands())
            CHECK_EQ(invoke<int64_t>(sub, value), static_cast<int64_t>(value - immediate));
    }
}

void testMultiplySubSignExtend32()
{
    // d = a - SExt32(n) * SExt32(m)
    auto sub = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplySubSignExtend32(GPRInfo::argumentGPR1, 
            GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR0, 
            GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands())
                CHECK_EQ(invoke<int64_t>(sub, a, n, m), a - static_cast<int64_t>(n) * static_cast<int64_t>(m));
        }
    }
}

void testMultiplySubZeroExtend32()
{
    // d = a - (ZExt32(n) * ZExt32(m))
    auto sub = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplySubZeroExtend32(GPRInfo::argumentGPR1, 
            GPRInfo::argumentGPR2,
            GPRInfo::argumentGPR0, 
            GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands()) {
                uint32_t un = n;
                uint32_t um = m;
                CHECK_EQ(invoke<int64_t>(sub, a, n, m), a - static_cast<int64_t>(un) * static_cast<int64_t>(um));
            }
        }
    }
}

void testMultiplyNegSignExtend32()
{
    // d = - (SExt32(n) * SExt32(m))
    auto neg = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplyNegSignExtend32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto n : int32Operands()) {
        for (auto m : int32Operands())
            CHECK_EQ(invoke<int64_t>(neg, n, m), -(static_cast<int64_t>(n) * static_cast<int64_t>(m)));
    }
}

void testMultiplyNegZeroExtend32()
{
    // d = - ZExt32(n) * ZExt32(m)
    auto neg = compile([=] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.multiplyNegZeroExtend32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            uint32_t un = n;
            uint32_t um = m;
            CHECK_EQ(invoke<uint64_t>(neg, n, m), -(static_cast<uint64_t>(un) * static_cast<uint64_t>(um)));
        }
    }
}

void testExtractUnsignedBitfield32()
{
    uint32_t src = 0xf0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (width > 0 && lsb + width < 32) {
                auto ubfx32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractUnsignedBitfield32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                CHECK_EQ(invoke<uint32_t>(ubfx32, src), ((src >> lsb) & ((1U << width) - 1U)));
            }
        }
    }
}

void testExtractUnsignedBitfield64()
{
    uint64_t src = 0xf0f0f0f0f0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto ubfx64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractUnsignedBitfield64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                CHECK_EQ(invoke<uint64_t>(ubfx64, src), ((src >> lsb) & ((1ULL << width) - 1ULL)));
            }
        }
    }
}

void testInsertUnsignedBitfieldInZero32()
{
    uint32_t src = 0xf0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto ubfiz32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertUnsignedBitfieldInZero32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint32_t mask = (1U << width) - 1U;
                CHECK_EQ(invoke<uint32_t>(ubfiz32, src), (src & mask) << lsb);
            }
        }
    }
}

void testInsertUnsignedBitfieldInZero64()
{
    uint64_t src = 0xf0f0f0f0f0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto ubfiz64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertUnsignedBitfieldInZero64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint64_t mask = (1ULL << width) - 1ULL;
                CHECK_EQ(invoke<uint64_t>(ubfiz64, src), (src & mask) << lsb);
            }
        }
    }
}

void testInsertBitField32()
{
    uint32_t src = 0x0f0f0f0f;
    uint32_t dst = 0xf0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto bfi32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertBitField32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::argumentGPR1);
                    jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint32_t mask1 = (1U << width) - 1U;
                uint32_t mask2 = ~(mask1 << lsb);
                uint32_t rhs = invoke<uint32_t>(bfi32, src, dst);
                uint32_t lhs = ((src & mask1) << lsb) | (dst & mask2);
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testInsertBitField64()
{
    uint64_t src = 0x0f0f0f0f0f0f0f0f;
    uint64_t dst = 0xf0f0f0f0f0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto bfi64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertBitField64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::argumentGPR1);
                    jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint64_t mask1 = (1ULL << width) - 1ULL;
                uint64_t mask2 = ~(mask1 << lsb);
                uint64_t rhs = invoke<uint64_t>(bfi64, src, dst);
                uint64_t lhs = ((src & mask1) << lsb) | (dst & mask2);
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testExtractInsertBitfieldAtLowEnd32()
{
    uint32_t src = 0xf0f0f0f0;
    uint32_t dst = 0x0f0f0f0f;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto bfxil32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractInsertBitfieldAtLowEnd32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::argumentGPR1);
                    jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint32_t mask1 = (1U << width) - 1U;
                uint32_t mask2 = ~mask1;
                uint32_t rhs = invoke<uint32_t>(bfxil32, src, dst);
                uint32_t lhs = ((src >> lsb) & mask1) | (dst & mask2);
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testExtractInsertBitfieldAtLowEnd64()
{
    uint64_t src = 0x0f0f0f0f0f0f0f0f;
    uint64_t dst = 0xf0f0f0f0f0f0f0f0;
    Vector<uint64_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto bfxil64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractInsertBitfieldAtLowEnd64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::argumentGPR1);
                    jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint64_t mask1 = (1ULL << width) - 1ULL;
                uint64_t mask2 = ~mask1;
                uint64_t rhs = invoke<uint64_t>(bfxil64, src, dst);
                uint64_t lhs = ((src >> lsb) & mask1) | (dst & mask2);
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testClearBitField32()
{
    uint32_t src = std::numeric_limits<uint32_t>::max();
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto bfc32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.clearBitField32(CCallHelpers::TrustedImm32(lsb), CCallHelpers::TrustedImm32(width), GPRInfo::argumentGPR0);
                    jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint32_t mask = ((1U << width) - 1U) << lsb;
                uint32_t rhs = invoke<uint32_t>(bfc32, src);
                uint32_t lhs = src & ~mask;
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testClearBitField64()
{
    uint64_t src = std::numeric_limits<uint64_t>::max();
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto bfc64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.clearBitField64(CCallHelpers::TrustedImm32(lsb), CCallHelpers::TrustedImm32(width), GPRInfo::argumentGPR0);
                    jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });
                uint64_t mask = ((1ULL << width) - 1ULL) << lsb;
                uint64_t rhs = invoke<uint64_t>(bfc64, src);
                uint64_t lhs = src & ~mask;
                CHECK_EQ(rhs, lhs);
            }
        }
    }
}

void testClearBitsWithMask32()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.clearBitsWithMask32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int32Operands()) {
        uint32_t src = std::numeric_limits<uint32_t>::max();
        CHECK_EQ(invoke<uint32_t>(test, src, mask), (src & ~mask));
        CHECK_EQ(invoke<uint32_t>(test, 0U, mask), 0U);
    }
}

void testClearBitsWithMask64()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.clearBitsWithMask64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int64Operands()) {
        uint64_t src = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<uint64_t>(test, src, mask), (src & ~mask));
        CHECK_EQ(invoke<uint64_t>(test, 0ULL, mask), 0ULL);
    }
}

void testOrNot32()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.orNot32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int32Operands()) {
        int32_t src = std::numeric_limits<uint32_t>::max();
        CHECK_EQ(invoke<int32_t>(test, src, mask), (src | ~mask));
        CHECK_EQ(invoke<int32_t>(test, 0U, mask), ~mask);
    }
}

void testOrNot64()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.orNot64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int64Operands()) {
        int64_t src = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<int64_t>(test, src, mask), (src | ~mask));
        CHECK_EQ(invoke<int64_t>(test, 0ULL, mask), ~mask);
    }
}

void testInsertSignedBitfieldInZero32()
{
    uint32_t src = 0xf0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto insertSignedBitfieldInZero32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertSignedBitfieldInZero32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t bf = src;
                int32_t mask1 = (1 << width) - 1;
                int32_t mask2 = 1 << (width - 1);
                int32_t bfsx = ((bf & mask1) ^ mask2) - mask2;

                CHECK_EQ(invoke<int32_t>(insertSignedBitfieldInZero32, src), bfsx << lsb);
            }
        }
    }
}

void testInsertSignedBitfieldInZero64()
{
    int64_t src = 0xf0f0f0f0f0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto insertSignedBitfieldInZero64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.insertSignedBitfieldInZero64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t bf = src;
                int64_t amount = CHAR_BIT * sizeof(bf) - width;
                int64_t bfsx = (bf << amount) >> amount;

                CHECK_EQ(invoke<int64_t>(insertSignedBitfieldInZero64, src), bfsx << lsb);
            }
        }
    }
}

void testExtractSignedBitfield32()
{
    int32_t src = 0xf0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 32) {
                auto extractSignedBitfield32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractSignedBitfield32(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t bf = src >> lsb;
                int32_t mask1 = (1 << width) - 1;
                int32_t mask2 = 1 << (width - 1);
                int32_t bfsx = ((bf & mask1) ^ mask2) - mask2;

                CHECK_EQ(invoke<int32_t>(extractSignedBitfield32, src), bfsx);
            }
        }
    }
}

void testExtractSignedBitfield64()
{
    int64_t src = 0xf0f0f0f0f0f0f0f0;
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    for (auto lsb : imms) {
        for (auto width : imms) {
            if (lsb >= 0 && width > 0 && lsb + width < 64) {
                auto extractSignedBitfield64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.extractSignedBitfield64(GPRInfo::argumentGPR0, 
                        CCallHelpers::TrustedImm32(lsb), 
                        CCallHelpers::TrustedImm32(width), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t bf = src >> lsb;
                int64_t amount = CHAR_BIT * sizeof(bf) - width;
                int64_t bfsx = (bf << amount) >> amount;

                CHECK_EQ(invoke<int64_t>(extractSignedBitfield64, src), bfsx);
            }
        }
    }
}

void testExtractRegister32()
{
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    uint32_t datasize = CHAR_BIT * sizeof(uint32_t);

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto lsb : imms) {
                if (0 <= lsb && lsb < datasize) {
                    auto extractRegister32 = compile([=] (CCallHelpers& jit) {
                        emitFunctionPrologue(jit);

                        jit.extractRegister32(GPRInfo::argumentGPR0, 
                            GPRInfo::argumentGPR1, 
                            CCallHelpers::TrustedImm32(lsb), 
                            GPRInfo::returnValueGPR);

                        emitFunctionEpilogue(jit);
                        jit.ret();
                    });

                    // ((n & mask) << highWidth) | (m >> lowWidth)
                    // Where: highWidth = datasize - lowWidth
                    //        mask = (1 << lowWidth) - 1
                    uint32_t highWidth = datasize - lsb;
                    uint32_t mask = (1U << lsb) - 1U;
                    uint32_t left = highWidth == datasize ? 0U : (n & mask) << highWidth;
                    uint32_t right = (static_cast<uint32_t>(m) >> lsb);
                    uint32_t rhs = left | right;
                    uint32_t lhs = invoke<uint32_t>(extractRegister32, n, m);
                    CHECK_EQ(lhs, rhs);
                }
            }
        }
    }
}

void testExtractRegister64()
{
    Vector<uint32_t> imms = { 0, 1, 5, 7, 30, 31, 32, 42, 56, 62, 63, 64 };
    uint64_t datasize = CHAR_BIT * sizeof(uint64_t);

    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto lsb : imms) {
                if (0 <= lsb && lsb < datasize) {
                    auto extractRegister64 = compile([=] (CCallHelpers& jit) {
                        emitFunctionPrologue(jit);

                        jit.extractRegister64(GPRInfo::argumentGPR0, 
                            GPRInfo::argumentGPR1, 
                            CCallHelpers::TrustedImm32(lsb), 
                            GPRInfo::returnValueGPR);

                        emitFunctionEpilogue(jit);
                        jit.ret();
                    });

                    uint64_t highWidth = datasize - lsb;
                    uint64_t mask = (1ULL << lsb) - 1ULL;
                    uint64_t left = highWidth == datasize ? 0ULL : (n & mask) << highWidth;
                    uint64_t right = (static_cast<uint64_t>(m) >> lsb);
                    uint64_t rhs = left | right;
                    uint64_t lhs = invoke<uint64_t>(extractRegister64, n, m);
                    CHECK_EQ(lhs, rhs);
                }
            }
        }
    }
}

void testAddWithLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto add32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addLeftShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(add32, n, m);
                int32_t rhs = n + (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAddWithRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto add32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(add32, n, m);
                int32_t rhs = n + (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAddWithUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto add32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addUnsignedRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(add32, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) + (static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAddWithLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto add64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addLeftShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(add64, n, m);
                int64_t rhs = n + (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAddWithRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto add64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(add64, n, m);
                int64_t rhs = n + (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAddWithUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto add64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.addUnsignedRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(add64, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) + (static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto sub32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subLeftShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(sub32, n, m);
                int32_t rhs = n - (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto sub32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(sub32, n, m);
                int32_t rhs = n - (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto sub32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subUnsignedRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(sub32, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) - (static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto sub64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subLeftShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(sub64, n, m);
                int64_t rhs = n - (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto sub64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(sub64, n, m);
                int64_t rhs = n - (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testSubWithUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto sub64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.subUnsignedRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(sub64, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) - (static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNot32()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.xorNot32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int32Operands()) {
        int32_t src = std::numeric_limits<uint32_t>::max();
        CHECK_EQ(invoke<int32_t>(test, src, mask), (src ^ ~mask));
        CHECK_EQ(invoke<int32_t>(test, 0U, mask), ~mask);
    }
}

void testXorNot64()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.xorNot64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto mask : int64Operands()) {
        int64_t src = std::numeric_limits<uint64_t>::max();
        CHECK_EQ(invoke<int64_t>(test, src, mask), (src ^ ~mask));
        CHECK_EQ(invoke<int64_t>(test, 0ULL, mask), ~mask);
    }
}

void testXorNotWithLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotLeftShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(test, n, m);
                int32_t rhs = n ^ ~(m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNotWithRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(test, n, m);
                int32_t rhs = n ^ ~(m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNotWithUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotUnsignedRightShift32(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(test, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) ^ ~(static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNotWithLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotLeftShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(test, n, m);
                int64_t rhs = n ^ ~(m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNotWithRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(test, n, m);
                int64_t rhs = n ^ ~(m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorNotWithUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto test = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorNotUnsignedRightShift64(GPRInfo::argumentGPR0, 
                        GPRInfo::argumentGPR1, 
                        CCallHelpers::TrustedImm32(amount), 
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(test, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) ^ ~(static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testStorePrePostIndex32()
{
    int32_t nums[] = { 1, 2, 3 };
    intptr_t addr = bitwise_cast<intptr_t>(&nums[1]);
    int32_t index = sizeof(int32_t);

    auto test1 = [&] (int32_t src) {
        auto store = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // *++p1 = 4; return p1;
            jit.store32(GPRInfo::argumentGPR0, CCallHelpers::PreIndexAddress(GPRInfo::argumentGPR1, index));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<intptr_t>(store, src, addr);
    };

    int32_t* p1 = bitwise_cast<int32_t*>(test1(4));
    CHECK_EQ(*p1, 4);
    CHECK_EQ(*--p1, nums[1]);

    auto test2 = [&] (int32_t src) {
        auto store = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // *p2++ = 5; return p2;
            jit.store32(GPRInfo::argumentGPR0, CCallHelpers::PostIndexAddress(GPRInfo::argumentGPR1, index));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<intptr_t>(store, src, addr);
    };

    int32_t* p2 = bitwise_cast<int32_t*>(test2(5));
    CHECK_EQ(*p2, 4);
    CHECK_EQ(*--p2, 5);
}

void testStorePrePostIndex64()
{
    int64_t nums[] = { 1, 2, 3 };
    intptr_t addr = bitwise_cast<intptr_t>(&nums[1]);
    int32_t index = sizeof(int64_t);

    auto test1 = [&] (int64_t src) {
        auto store = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // *++p1 = 4; return p1;
            jit.store64(GPRInfo::argumentGPR0, CCallHelpers::PreIndexAddress(GPRInfo::argumentGPR1, index));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<intptr_t>(store, src, addr);
    };

    int64_t* p1 = bitwise_cast<int64_t*>(test1(4));
    CHECK_EQ(*p1, 4);
    CHECK_EQ(*--p1, nums[1]);

    auto test2 = [&] (int64_t src) {
        auto store = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // *p2++ = 5; return p2;
            jit.store64(GPRInfo::argumentGPR0, CCallHelpers::PostIndexAddress(GPRInfo::argumentGPR1, index));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<intptr_t>(store, src, addr);
    };

    int64_t* p2 = bitwise_cast<int64_t*>(test2(5));
    CHECK_EQ(*p2, 4);
    CHECK_EQ(*--p2, 5);
}

void testLoadPrePostIndex32()
{
    int32_t nums[] = { 1, 2, 3 };
    int32_t index = sizeof(int32_t);

    auto test1 = [&] (int32_t replace) {
        auto load = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // res = *++p1; *p1 = 4; return res;
            jit.load32(CCallHelpers::PreIndexAddress(GPRInfo::argumentGPR0, index), GPRInfo::argumentGPR1);
            jit.store32(CCallHelpers::TrustedImm32(replace), CCallHelpers::Address(GPRInfo::argumentGPR0));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<int32_t>(load, &nums[1]);
    };

    CHECK_EQ(test1(4), 3);
    CHECK_EQ(nums[2], 4);

    auto test2 = [&] (int32_t replace) {
        auto load = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // res = *p2++; *p2 = 5; return res;
            jit.load32(CCallHelpers::PostIndexAddress(GPRInfo::argumentGPR0, index), GPRInfo::argumentGPR1);
            jit.store32(CCallHelpers::TrustedImm32(replace), CCallHelpers::Address(GPRInfo::argumentGPR0));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<int32_t>(load, &nums[1]);
    };

    CHECK_EQ(test2(5), 2);
    CHECK_EQ(nums[2], 5);
}

void testLoadPrePostIndex64()
{
    int64_t nums[] = { 1, 2, 3 };
    int32_t index = sizeof(int64_t);

    auto test1 = [&] (int64_t replace) {
        auto load = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // res = *++p1; *p1 = 4; return res;
            jit.load64(CCallHelpers::PreIndexAddress(GPRInfo::argumentGPR0, index), GPRInfo::argumentGPR1);
            jit.store64(CCallHelpers::TrustedImm64(replace), CCallHelpers::Address(GPRInfo::argumentGPR0));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<int64_t>(load, &nums[1]);
    };

    CHECK_EQ(test1(4), 3);
    CHECK_EQ(nums[2], 4);

    auto test2 = [&] (int64_t replace) {
        auto load = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            // res = *p2++; *p2 = 5; return res;
            jit.load64(CCallHelpers::PostIndexAddress(GPRInfo::argumentGPR0, index), GPRInfo::argumentGPR1);
            jit.store64(CCallHelpers::TrustedImm64(replace), CCallHelpers::Address(GPRInfo::argumentGPR0));
            jit.move(GPRInfo::argumentGPR1, GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });
        return invoke<int64_t>(load, &nums[1]);
    };

    CHECK_EQ(test2(5), 2);
    CHECK_EQ(nums[2], 5);
}

void testAndLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto and32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andLeftShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(and32, n, m);
                int32_t rhs = n & (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAndRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto and32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(and32, n, m);
                int32_t rhs = n & (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAndUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto and32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andUnsignedRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(and32, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) & (static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAndLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto and64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andLeftShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(and64, n, m);
                int64_t rhs = n & (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAndRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto and64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(and64, n, m);
                int64_t rhs = n & (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testAndUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto and64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.andUnsignedRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(and64, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) & (static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto xor32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorLeftShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(xor32, n, m);
                int32_t rhs = n ^ (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto xor32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(xor32, n, m);
                int32_t rhs = n ^ (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto xor32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorUnsignedRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(xor32, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) ^ (static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto xor64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorLeftShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(xor64, n, m);
                int64_t rhs = n ^ (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto xor64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(xor64, n, m);
                int64_t rhs = n ^ (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testXorUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto xor64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.xorUnsignedRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(xor64, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) ^ (static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrLeftShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto or32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orLeftShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(or32, n, m);
                int32_t rhs = n | (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrRightShift32()
{
    Vector<int32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto or32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int32_t lhs = invoke<int32_t>(or32, n, m);
                int32_t rhs = n | (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrUnsignedRightShift32()
{
    Vector<uint32_t> amounts = { 0, 17, 31 };
    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto amount : amounts) {
                auto or32 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orUnsignedRightShift32(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint32_t lhs = invoke<uint32_t>(or32, n, m);
                uint32_t rhs = static_cast<uint32_t>(n) | (static_cast<uint32_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrLeftShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto or64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orLeftShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(or64, n, m);
                int64_t rhs = n | (m << amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrRightShift64()
{
    Vector<int32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto or64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                int64_t lhs = invoke<int64_t>(or64, n, m);
                int64_t rhs = n | (m >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}

void testOrUnsignedRightShift64()
{
    Vector<uint32_t> amounts = { 0, 34, 63 };
    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            for (auto amount : amounts) {
                auto or64 = compile([=] (CCallHelpers& jit) {
                    emitFunctionPrologue(jit);

                    jit.orUnsignedRightShift64(GPRInfo::argumentGPR0,
                        GPRInfo::argumentGPR1,
                        CCallHelpers::TrustedImm32(amount),
                        GPRInfo::returnValueGPR);

                    emitFunctionEpilogue(jit);
                    jit.ret();
                });

                uint64_t lhs = invoke<uint64_t>(or64, n, m);
                uint64_t rhs = static_cast<uint64_t>(n) | (static_cast<uint64_t>(m) >> amount);
                CHECK_EQ(lhs, rhs);
            }
        }
    }
}
#endif

#if CPU(X86) || CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
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

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)

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

#endif // CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)

#if CPU(ARM64E)

void testAtomicStrongCASFill8()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.atomicStrongCAS8(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::Address(GPRInfo::argumentGPR2));
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    uint8_t data[] = {
        0xff, 0xff,
    };
    uint32_t result = invoke<uint32_t>(test, 0xffffffffffffffffULL, 0, data);
    CHECK_EQ(result, 0xff);
    CHECK_EQ(data[0], 0);
    CHECK_EQ(data[1], 0xff);
}

void testAtomicStrongCASFill16()
{
    auto test = compile([] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        jit.atomicStrongCAS16(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::Address(GPRInfo::argumentGPR2));
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    uint16_t data[] = {
        0xffff, 0xffff,
    };
    uint32_t result = invoke<uint32_t>(test, 0xffffffffffffffffULL, 0, data);
    CHECK_EQ(result, 0xffff);
    CHECK_EQ(data[0], 0);
    CHECK_EQ(data[1], 0xffff);
}

#endif // CPU(ARM64)

void testLoadStorePair32()
{
    constexpr uint32_t initialValue = 0x55aabb80u;
    constexpr uint32_t value1 = 42;
    constexpr uint32_t value2 = 0xcfbb1357u;

    uint32_t buffer[10];

    auto initBuffer = [&] {
        for (unsigned i = 0; i < 10; ++i)
            buffer[i] = initialValue + i;
    };

    struct Pair {
        uint32_t value1;
        uint32_t value2;
    };

    Pair pair;
    auto initPair = [&] {
        pair = { 0, 0 };
    };

    // Test loadPair32.
    auto testLoadPair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg pairGPR = GPRInfo::argumentGPR1;
        jit.loadPair32(bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(uint32_t)), GPRInfo::regT2, GPRInfo::regT3);

        jit.store32(GPRInfo::regT2, CCallHelpers::Address(pairGPR, 0));
        jit.store32(GPRInfo::regT3, CCallHelpers::Address(pairGPR, sizeof(uint32_t)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testLoadPair0 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 0);
    });

    initBuffer();

    initPair();
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4);
    CHECK_EQ(pair.value2, initialValue + 5);

    initPair();
    buffer[4] = value1;
    buffer[5] = value2;
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    auto testLoadPairMinus2 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, -2);
    });

    initPair();
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 - 2);
    CHECK_EQ(pair.value2, initialValue + 5 - 2);

    initPair();
    buffer[4 - 2] = value2;
    buffer[5 - 2] = value1;
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value2);
    CHECK_EQ(pair.value2, value1);

    auto testLoadPairPlus3 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 3);
    });

    initPair();
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 + 3);
    CHECK_EQ(pair.value2, initialValue + 5 + 3);

    initPair();
    buffer[4 + 3] = value1;
    buffer[5 + 3] = value2;
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    // Test loadPair32 using a buffer register as a destination.
    auto testLoadPairUsingBufferRegisterAsDestination = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg pairGPR = GPRInfo::argumentGPR1;
        jit.loadPair32(bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(uint32_t)), GPRInfo::argumentGPR0, GPRInfo::regT2);

        jit.store32(GPRInfo::argumentGPR0, CCallHelpers::Address(pairGPR, 0));
        jit.store32(GPRInfo::regT2, CCallHelpers::Address(pairGPR, sizeof(uint32_t)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testLoadPairUsingBufferRegisterAsDestination0 = compile([&] (CCallHelpers& jit) {
        testLoadPairUsingBufferRegisterAsDestination(jit, 0);
    });

    initBuffer();

    initPair();
    invoke<void>(testLoadPairUsingBufferRegisterAsDestination0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4);
    CHECK_EQ(pair.value2, initialValue + 5);

    // Test storePair32.
    auto testStorePair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR2;
        jit.storePair32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(unsigned)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testStorePair0 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePair0, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value1);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairMinus2, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value2);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairPlus3, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value1);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);

    // Test storePair32 from 1 register.
    auto testStorePairFromOneReg = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR1;
        jit.storePair32(GPRInfo::argumentGPR0, GPRInfo::argumentGPR0, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(unsigned)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testStorePairFromOneReg0 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneReg0, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value2);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegMinus2, value1, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value1);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegPlus3, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value2);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);
}

void testSub32ArgImm()
{
    for (auto immediate : int32Operands()) {
        auto sub = compile([=] (CCallHelpers& jit) {
            emitFunctionPrologue(jit);

            jit.sub32(GPRInfo::argumentGPR0, CCallHelpers::TrustedImm32(immediate), GPRInfo::returnValueGPR);

            emitFunctionEpilogue(jit);
            jit.ret();
        });

        for (auto value : int32Operands())
            CHECK_EQ(invoke<uint32_t>(sub, value), static_cast<uint32_t>(value - immediate));
    }
}

#if CPU(ARM64)
void testLoadStorePair64Int64()
{
    constexpr uint64_t initialValue = 0x5555aaaabbbb8800ull;
    constexpr uint64_t value1 = 42;
    constexpr uint64_t value2 = 0xcafebabe12345678ull;

    uint64_t buffer[10];

    auto initBuffer = [&] {
        for (unsigned i = 0; i < 10; ++i)
            buffer[i] = initialValue + i;
    };

    struct Pair {
        uint64_t value1;
        uint64_t value2;
    };

    Pair pair;
    auto initPair = [&] {
        pair = { 0, 0 };
    };

    // Test loadPair64.
    auto testLoadPair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg pairGPR = GPRInfo::argumentGPR1;
        jit.loadPair64(bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(CPURegister)), GPRInfo::regT2, GPRInfo::regT3);

        jit.store64(GPRInfo::regT2, CCallHelpers::Address(pairGPR, 0));
        jit.store64(GPRInfo::regT3, CCallHelpers::Address(pairGPR, sizeof(uint64_t)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testLoadPair0 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 0);
    });

    initBuffer();

    initPair();
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4);
    CHECK_EQ(pair.value2, initialValue + 5);

    initPair();
    buffer[4] = value1;
    buffer[5] = value2;
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    auto testLoadPairMinus2 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, -2);
    });

    initPair();
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 - 2);
    CHECK_EQ(pair.value2, initialValue + 5 - 2);

    initPair();
    buffer[4 - 2] = value2;
    buffer[5 - 2] = value1;
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value2);
    CHECK_EQ(pair.value2, value1);

    auto testLoadPairPlus3 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 3);
    });

    initPair();
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 + 3);
    CHECK_EQ(pair.value2, initialValue + 5 + 3);

    initPair();
    buffer[4 + 3] = value1;
    buffer[5 + 3] = value2;
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    // Test loadPair64 using a buffer register as a destination.
    auto testLoadPairUsingBufferRegisterAsDestination = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg pairGPR = GPRInfo::argumentGPR1;
        jit.loadPair64(bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(CPURegister)), GPRInfo::argumentGPR0, GPRInfo::regT2);

        jit.store64(GPRInfo::argumentGPR0, CCallHelpers::Address(pairGPR, 0));
        jit.store64(GPRInfo::regT2, CCallHelpers::Address(pairGPR, sizeof(uint64_t)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testLoadPairUsingBufferRegisterAsDestination0 = compile([&] (CCallHelpers& jit) {
        testLoadPairUsingBufferRegisterAsDestination(jit, 0);
    });

    initBuffer();

    initPair();
    invoke<void>(testLoadPairUsingBufferRegisterAsDestination0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4);
    CHECK_EQ(pair.value2, initialValue + 5);

    // Test storePair64.
    auto testStorePair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR2;
        jit.storePair64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(CPURegister)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testStorePair0 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePair0, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value1);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairMinus2, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value2);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairPlus3, value1, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value1);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);

    // Test storePair64 from 1 register.
    auto testStorePairFromOneReg = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR1;
        jit.storePair64(GPRInfo::argumentGPR0, GPRInfo::argumentGPR0, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(CPURegister)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testStorePairFromOneReg0 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneReg0, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value2);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegMinus2, value1, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value1);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegPlus3, value2, &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value2);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);
}

void testLoadStorePair64Double()
{
    constexpr double initialValue = 10000.275;
    constexpr double value1 = 42.89;
    constexpr double value2 = -555.321;

    double buffer[10];

    auto initBuffer = [&] {
        for (unsigned i = 0; i < 10; ++i)
            buffer[i] = initialValue + i;
    };

    struct Pair {
        double value1;
        double value2;
    };

    Pair pair;
    auto initPair = [&] {
        pair = { 0, 0 };
    };

    // Test loadPair64.
    auto testLoadPair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg pairGPR = GPRInfo::argumentGPR1;
        jit.loadPair64(bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(double)), FPRInfo::fpRegT0, FPRInfo::fpRegT1);

        jit.storeDouble(FPRInfo::fpRegT0, CCallHelpers::Address(pairGPR, 0));
        jit.storeDouble(FPRInfo::fpRegT1, CCallHelpers::Address(pairGPR, sizeof(uint64_t)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testLoadPair0 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 0);
    });

    initBuffer();

    initPair();
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4);
    CHECK_EQ(pair.value2, initialValue + 5);

    initPair();
    buffer[4] = value1;
    buffer[5] = value2;
    invoke<void>(testLoadPair0, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    auto testLoadPairMinus2 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, -2);
    });

    initPair();
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 - 2);
    CHECK_EQ(pair.value2, initialValue + 5 - 2);

    initPair();
    buffer[4 - 2] = value2;
    buffer[5 - 2] = value1;
    invoke<void>(testLoadPairMinus2, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value2);
    CHECK_EQ(pair.value2, value1);

    auto testLoadPairPlus3 = compile([&] (CCallHelpers& jit) {
        testLoadPair(jit, 3);
    });

    initPair();
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, initialValue + 4 + 3);
    CHECK_EQ(pair.value2, initialValue + 5 + 3);

    initPair();
    buffer[4 + 3] = value1;
    buffer[5 + 3] = value2;
    invoke<void>(testLoadPairPlus3, &buffer[4], &pair);
    CHECK_EQ(pair.value1, value1);
    CHECK_EQ(pair.value2, value2);

    // Test storePair64.
    auto testStorePair = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR2;
        jit.move64ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT0);
        jit.move64ToDouble(GPRInfo::argumentGPR1, FPRInfo::fpRegT1);
        jit.storePair64(FPRInfo::fpRegT0, FPRInfo::fpRegT1, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(double)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto asInt64 = [] (double value) {
        return bitwise_cast<int64_t>(value);
    };

    auto testStorePair0 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePair0, asInt64(value1), asInt64(value2), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value1);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairMinus2, asInt64(value1), asInt64(value2), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value2);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePair(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairPlus3, asInt64(value1), asInt64(value2), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value1);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);

    // Test storePair64 from 1 register.
    auto testStorePairFromOneReg = [] (CCallHelpers& jit, int offset) {
        emitFunctionPrologue(jit);

        constexpr GPRReg bufferGPR = GPRInfo::argumentGPR1;
        jit.move64ToDouble(GPRInfo::argumentGPR0, FPRInfo::fpRegT0);
        jit.storePair64(FPRInfo::fpRegT0, FPRInfo::fpRegT0, bufferGPR, CCallHelpers::TrustedImm32(offset * sizeof(double)));

        emitFunctionEpilogue(jit);
        jit.ret();
    };

    auto testStorePairFromOneReg0 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 0);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneReg0, asInt64(value2), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], value2);
    CHECK_EQ(buffer[5], value2);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegMinus2 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, -2);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegMinus2, asInt64(value1), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], value1);
    CHECK_EQ(buffer[3], value1);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], initialValue + 7);
    CHECK_EQ(buffer[8], initialValue + 8);
    CHECK_EQ(buffer[9], initialValue + 9);

    auto testStorePairFromOneRegPlus3 = compile([&] (CCallHelpers& jit) {
        testStorePairFromOneReg(jit, 3);
    });

    initBuffer();
    invoke<void>(testStorePairFromOneRegPlus3, asInt64(value2), &buffer[4]);
    CHECK_EQ(buffer[0], initialValue + 0);
    CHECK_EQ(buffer[1], initialValue + 1);
    CHECK_EQ(buffer[2], initialValue + 2);
    CHECK_EQ(buffer[3], initialValue + 3);
    CHECK_EQ(buffer[4], initialValue + 4);
    CHECK_EQ(buffer[5], initialValue + 5);
    CHECK_EQ(buffer[6], initialValue + 6);
    CHECK_EQ(buffer[7], value2);
    CHECK_EQ(buffer[8], value2);
    CHECK_EQ(buffer[9], initialValue + 9);
}
#endif // CPU(ARM64)

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

        jit.probeDebug([&] (Probe::Context& context) {
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
        jit.probeDebug([&] (Probe::Context& context) {
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
        jit.probeDebug([&] (Probe::Context& context) {
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
        jit.probeDebug([&] (Probe::Context& context) {
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
        jit.probeDebug([&] (Probe::Context&) {
            probeCallCount++;
            CHECK_EQ(testFunctionToTrashGPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 10);
            CHECK_EQ(testFunctionToTrashFPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 10);
        });

        // Validate that the registers have the expected values.
        jit.probeDebug([&] (Probe::Context& context) {
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
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
#if CPU(MIPS)
                if (!(id & 1))
#endif
                CHECK_EQ(cpu.fpr<uint64_t>(id), testWord64(id));
            }
        });

        // Restore the original state.
        jit.probeDebug([&] (Probe::Context& context) {
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
        jit.probeDebug([&] (Probe::Context& context) {
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
#if !(CPU(MIPS) || CPU(RISCV64))
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
        jit.probeDebug([&] (Probe::Context& context) {
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

#if !(CPU(MIPS) || CPU(RISCV64))
            originalState.spr(flagsSPR) = cpu.spr(flagsSPR);
            modifiedFlags = originalState.spr(flagsSPR) ^ flagsMask;
            cpu.spr(flagsSPR) = modifiedFlags;
#endif

            originalSP = cpu.sp();
            modifiedSP = computeModifiedStackPointer(context);
            cpu.sp() = modifiedSP;
        });

        // Validate that the registers have the expected values.
        jit.probeDebug([&] (Probe::Context& context) {
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
#if !(CPU(MIPS) || CPU(RISCV64))
            CHECK_EQ(cpu.spr(flagsSPR) & flagsMask, modifiedFlags & flagsMask);
#endif
            CHECK_EQ(cpu.sp(), modifiedSP);
        });

        // Restore the original state.
        jit.probeDebug([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                cpu.fpr(id) = originalState.fpr(id);
#if !(CPU(MIPS) || CPU(RISCV64))
            cpu.spr(flagsSPR) = originalState.spr(flagsSPR);
#endif
            cpu.sp() = originalSP;
        });

        // Validate that the original state was restored.
        jit.probeDebug([&] (Probe::Context& context) {
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
#if !(CPU(MIPS) || CPU(RISCV64))
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
        jit.probeDebug([&] (Probe::Context&) {
            probeCallCount++;
            continuationWasReached = true;
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    compileAndRun<void>([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);

        // Write expected values into the registers.
        jit.probeDebug([&] (Probe::Context& context) {
            probeCallCount++;
            context.cpu.pc() = retagCodePtr<JSEntryPtrTag, JITProbePCPtrTag>(continuation.code().taggedPtr());
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
#if !(CPU(MIPS) || CPU(RISCV64))
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
        jit.probeDebug([&] (Probe::Context& context) {
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
#if !(CPU(MIPS) || CPU(RISCV64))
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
        jit.probeDebug([&] (Probe::Context& context) {
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
#if !(CPU(MIPS) || CPU(RISCV64))
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
        jit.probeDebug([&] (Probe::Context& context) {
            auto& cpu = context.cpu;
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                cpu.gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                cpu.fpr(id) = originalState.fpr(id);
#if !(CPU(MIPS) || CPU(RISCV64))
            cpu.spr(flagsSPR) = originalState.spr(flagsSPR);
#endif
            cpu.sp() = originalSP;
        });

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    CHECK_EQ(probeCallCount, 3);
}

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
    auto or8 = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.or8(CCallHelpers::TrustedImm32(42), CCallHelpers::AbsoluteAddress(&memoryLocation));
        emitFunctionEpilogue(jit);
        jit.ret();
    });
    invoke<void>(or8);
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

void testAndOrDouble()
{
    double arg1, arg2;

    auto andDouble = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT1);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT2);

        jit.andDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT2, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    auto operands = doubleOperands();
    for (auto a : operands) {
        for (auto b : operands) {
            arg1 = a;
            arg2 = b;
            uint64_t expectedResult = bitwise_cast<uint64_t>(arg1) & bitwise_cast<uint64_t>(arg2);
            CHECK_EQ(bitwise_cast<uint64_t>(invoke<double>(andDouble)), expectedResult);
        }
    }

    auto orDouble = compile([&] (CCallHelpers& jit) {
        emitFunctionPrologue(jit);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg1), FPRInfo::fpRegT1);
        jit.loadDouble(CCallHelpers::TrustedImmPtr(&arg2), FPRInfo::fpRegT2);

        jit.orDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT2, FPRInfo::returnValueFPR);

        emitFunctionEpilogue(jit);
        jit.ret();
    });

    for (auto a : operands) {
        for (auto b : operands) {
            arg1 = a;
            arg2 = b;
            uint64_t expectedResult = bitwise_cast<uint64_t>(arg1) | bitwise_cast<uint64_t>(arg2);
            CHECK_EQ(bitwise_cast<uint64_t>(invoke<double>(orDouble)), expectedResult);
        }
    }
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

void testLoadBaseIndex()
{
#if CPU(ARM64) || CPU(X86_64) || CPU(RISCV64)
    // load64
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load64(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, -8), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint64_t array[] = { UINT64_MAX - 1, UINT64_MAX - 2, UINT64_MAX - 3, UINT64_MAX - 4, UINT64_MAX - 5, };
        CHECK_EQ(invoke<uint64_t>(test, array, static_cast<UCPURegister>(3)), UINT64_MAX - 3);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load64(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, 8), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint64_t array[] = { UINT64_MAX - 1, UINT64_MAX - 2, UINT64_MAX - 3, UINT64_MAX - 4, UINT64_MAX - 5, };
        CHECK_EQ(invoke<uint64_t>(test, array, static_cast<UCPURegister>(3)), UINT64_MAX - 5);
    }
#endif

    // load32
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, -4), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint32_t array[] = { UINT32_MAX - 1, UINT32_MAX - 2, UINT32_MAX - 3, UINT32_MAX - 4, UINT32_MAX - 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), UINT32_MAX - 3);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, 4), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint32_t array[] = { UINT32_MAX - 1, UINT32_MAX - 2, UINT32_MAX - 3, UINT32_MAX - 4, UINT32_MAX - 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), UINT32_MAX - 5);
    }

    // load16
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load16(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, -2), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { UINT16_MAX - 1, UINT16_MAX - 2, UINT16_MAX - 3, UINT16_MAX - 4, UINT16_MAX - 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), UINT16_MAX - 3);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load16(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, 2), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { UINT16_MAX - 1, UINT16_MAX - 2, UINT16_MAX - 3, UINT16_MAX - 4, static_cast<uint16_t>(-1), };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), 0xffff);
    }

    // load16SignedExtendTo32
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load16SignedExtendTo32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, -2), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { 1, 2, 0x7ff3, 4, 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), 0x7ff3);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load16SignedExtendTo32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, 2), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { UINT16_MAX - 1, UINT16_MAX - 2, UINT16_MAX - 3, UINT16_MAX - 4, static_cast<uint16_t>(-1), };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), static_cast<uint32_t>(-1));
    }

    // load8
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load8(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, -1), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { UINT8_MAX - 1, UINT8_MAX - 2, UINT8_MAX - 3, UINT8_MAX - 4, UINT8_MAX - 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), UINT8_MAX - 3);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load8(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, 1), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { UINT8_MAX - 1, UINT8_MAX - 2, UINT8_MAX - 3, UINT8_MAX - 4, static_cast<uint8_t>(-1), };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), 0xff);
    }

    // load8SignedExtendTo32
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load8SignedExtendTo32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, -1), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { 1, 2, 0x73, 4, 5, };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), 0x73);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.load8SignedExtendTo32(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, 1), GPRInfo::returnValueGPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { UINT8_MAX - 1, UINT8_MAX - 2, UINT8_MAX - 3, UINT8_MAX - 4, static_cast<uint8_t>(-1), };
        CHECK_EQ(invoke<uint32_t>(test, array, static_cast<UCPURegister>(3)), static_cast<uint32_t>(-1));
    }

    // loadDouble
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.loadDouble(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, -8), FPRInfo::returnValueFPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        double array[] = { 1, 2, 3, 4, 5, };
        CHECK_EQ(invoke<double>(test, array, static_cast<UCPURegister>(3)), 3.0);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.loadDouble(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, 8), FPRInfo::returnValueFPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        double array[] = { 1, 2, 3, 4, 5, };
        CHECK_EQ(invoke<double>(test, array, static_cast<UCPURegister>(3)), 5.0);
    }

    // loadFloat
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.loadFloat(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, -4), FPRInfo::returnValueFPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        float array[] = { 1, 2, 3, 4, 5, };
        CHECK_EQ(invoke<float>(test, array, static_cast<UCPURegister>(3)), 3.0);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.loadFloat(CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, 4), FPRInfo::returnValueFPR);
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        float array[] = { 1, 2, 3, 4, 5, };
        CHECK_EQ(invoke<float>(test, array, static_cast<UCPURegister>(3)), 5.0);
    }
}

void testStoreBaseIndex()
{
#if CPU(ARM64) || CPU(X86_64) || CPU(RISCV64)
    // store64
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store64(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, -8));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint64_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT64_MAX - 42);
        CHECK_EQ(array[2], UINT64_MAX - 42);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store64(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, 8));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint64_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT64_MAX - 42);
        CHECK_EQ(array[4], UINT64_MAX - 42);
    }
#endif

    // store32
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store32(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, -4));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint32_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT32_MAX - 42);
        CHECK_EQ(array[2], UINT32_MAX - 42);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store32(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, 4));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint32_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT32_MAX - 42);
        CHECK_EQ(array[4], UINT32_MAX - 42);
    }

    // store16
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store16(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, -2));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT16_MAX - 42);
        CHECK_EQ(array[2], UINT16_MAX - 42);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store16(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesTwo, 2));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint16_t array[] = { 1, 2, 3, 4, static_cast<uint16_t>(-1), };
        invoke<void>(test, array, 3, UINT16_MAX - 42);
        CHECK_EQ(array[4], UINT16_MAX - 42);
    }

    // store8
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store8(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, -1));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, 3, UINT8_MAX - 42);
        CHECK_EQ(array[2], UINT8_MAX - 42);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.store8(GPRInfo::argumentGPR2, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesOne, 1));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        uint8_t array[] = { 1, 2, 3, 4, static_cast<uint8_t>(-1), };
        invoke<void>(test, array, 3, UINT8_MAX - 42);
        CHECK_EQ(array[4], UINT8_MAX - 42);
    }

    // storeDouble
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.storeDouble(FPRInfo::argumentFPR0, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, -8));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        double array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, static_cast<UCPURegister>(3), 42.0);
        CHECK_EQ(array[2], 42.0);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.storeDouble(FPRInfo::argumentFPR0, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesEight, 8));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        double array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, static_cast<UCPURegister>(3), 42.0);
        CHECK_EQ(array[4], 42.0);
    }

    // storeFloat
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.storeFloat(FPRInfo::argumentFPR0, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, -4));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        float array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, static_cast<UCPURegister>(3), 42.0f);
        CHECK_EQ(array[2], 42.0f);
    }
    {
        auto test = compile([=](CCallHelpers& jit) {
            emitFunctionPrologue(jit);
            jit.storeFloat(FPRInfo::argumentFPR0, CCallHelpers::BaseIndex(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, CCallHelpers::TimesFour, 4));
            emitFunctionEpilogue(jit);
            jit.ret();
        });
        float array[] = { 1, 2, 3, 4, 5, };
        invoke<void>(test, array, static_cast<UCPURegister>(3), 42.0f);
        CHECK_EQ(array[4], 42.0f);
    }
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
        constexpr GPRReg storageGPR = GPRInfo::argumentGPR0;
        constexpr GPRReg lengthGPR = GPRInfo::argumentGPR1;
        constexpr GPRReg scratchGPR = GPRInfo::argumentGPR2;
        jit.cageConditionallyAndUntag(Gigacage::Primitive, storageGPR, lengthGPR, scratchGPR);
        jit.move(GPRInfo::argumentGPR0, GPRInfo::returnValueGPR);
        emitFunctionEpilogue(jit);
        jit.ret();
    });

    void* ptr = Gigacage::tryMalloc(Gigacage::Primitive, 1);
    void* taggedPtr = tagArrayPtr(ptr, 1);
    RELEASE_ASSERT(hasOneBitSet(Gigacage::maxSize(Gigacage::Primitive) << 2));
    void* notCagedPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + (Gigacage::maxSize(Gigacage::Primitive) << 2));
    CHECK_NOT_EQ(Gigacage::caged(Gigacage::Primitive, notCagedPtr), notCagedPtr);
    void* taggedNotCagedPtr = tagArrayPtr(notCagedPtr, 1);

    if (!isARM64E())
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

static void testGPRInfoConsistency()
{
    for (unsigned index = 0; index < GPRInfo::numberOfRegisters; ++index) {
        GPRReg reg = GPRInfo::toRegister(index);
        CHECK_EQ(GPRInfo::toIndex(reg), index);
    }
    for (auto reg = CCallHelpers::firstRegister(); reg <= CCallHelpers::lastRegister(); reg = nextID(reg)) {
        if (isSpecialGPR(reg))
            continue;
        unsigned index = GPRInfo::toIndex(reg);
        if (index == GPRInfo::InvalidIndex) {
            CHECK_EQ(index >= GPRInfo::numberOfRegisters, true);
            continue;
        }
        CHECK_EQ(index < GPRInfo::numberOfRegisters, true);
    }
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

// Using WTF_IGNORES_THREAD_SAFETY_ANALYSIS because the function is still holding crashLock when exiting.
void run(const char* filter) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    JSC::initialize();
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
    RUN(testLoadStorePair32());
    RUN(testSub32ArgImm());

    RUN(testBranchTest8());
    RUN(testBranchTest16());

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
    RUN(testShiftAndAdd());
    RUN(testStore64Imm64AddressPointer());
#endif

#if CPU(ARM64)
    RUN(testLoadStorePair64Int64());
    RUN(testLoadStorePair64Double());
    RUN(testMultiplySignExtend32());
    RUN(testMultiplyZeroExtend32());

    RUN(testSub32Args());
    RUN(testSub32Imm());
    RUN(testSub64Imm32());
    RUN(testSub64ArgImm32());
    RUN(testSub64Imm64());
    RUN(testSub64ArgImm64());

    RUN(testMultiplyAddSignExtend32());
    RUN(testMultiplyAddZeroExtend32());
    RUN(testMultiplySubSignExtend32());
    RUN(testMultiplySubZeroExtend32());
    RUN(testMultiplyNegSignExtend32());
    RUN(testMultiplyNegZeroExtend32());

    RUN(testExtractUnsignedBitfield32());
    RUN(testExtractUnsignedBitfield64());
    RUN(testInsertUnsignedBitfieldInZero32());
    RUN(testInsertUnsignedBitfieldInZero64());
    RUN(testInsertBitField32());
    RUN(testInsertBitField64());
    RUN(testExtractInsertBitfieldAtLowEnd32());
    RUN(testExtractInsertBitfieldAtLowEnd64());
    RUN(testClearBitField32());
    RUN(testClearBitField64());
    RUN(testClearBitsWithMask32());
    RUN(testClearBitsWithMask64());

    RUN(testOrNot32());
    RUN(testOrNot64());

    RUN(testInsertSignedBitfieldInZero32());
    RUN(testInsertSignedBitfieldInZero64());
    RUN(testExtractSignedBitfield32());
    RUN(testExtractSignedBitfield64());
    RUN(testExtractRegister32());
    RUN(testExtractRegister64());

    RUN(testAddWithLeftShift32());
    RUN(testAddWithRightShift32());
    RUN(testAddWithUnsignedRightShift32());
    RUN(testAddWithLeftShift64());
    RUN(testAddWithRightShift64());
    RUN(testAddWithUnsignedRightShift64());
    RUN(testSubWithLeftShift32());
    RUN(testSubWithRightShift32());
    RUN(testSubWithUnsignedRightShift32());
    RUN(testSubWithLeftShift64());
    RUN(testSubWithRightShift64());
    RUN(testSubWithUnsignedRightShift64());

    RUN(testXorNot32());
    RUN(testXorNot64());
    RUN(testXorNotWithLeftShift32());
    RUN(testXorNotWithRightShift32());
    RUN(testXorNotWithUnsignedRightShift32());
    RUN(testXorNotWithLeftShift64());
    RUN(testXorNotWithRightShift64());
    RUN(testXorNotWithUnsignedRightShift64());

    RUN(testStorePrePostIndex32());
    RUN(testStorePrePostIndex64());
    RUN(testLoadPrePostIndex32());
    RUN(testLoadPrePostIndex64());
    RUN(testAndLeftShift32());
    RUN(testAndRightShift32());
    RUN(testAndUnsignedRightShift32());
    RUN(testAndLeftShift64());
    RUN(testAndRightShift64());
    RUN(testAndUnsignedRightShift64());

    RUN(testXorLeftShift32());
    RUN(testXorRightShift32());
    RUN(testXorUnsignedRightShift32());
    RUN(testXorLeftShift64());
    RUN(testXorRightShift64());
    RUN(testXorUnsignedRightShift64());

    RUN(testOrLeftShift32());
    RUN(testOrRightShift32());
    RUN(testOrUnsignedRightShift32());
    RUN(testOrLeftShift64());
    RUN(testOrRightShift64());
    RUN(testOrUnsignedRightShift64());
#endif

#if CPU(ARM64E)
    RUN(testAtomicStrongCASFill8());
    RUN(testAtomicStrongCASFill16());
#endif

#if CPU(X86) || CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    FOR_EACH_DOUBLE_CONDITION_RUN(testCompareFloat);
#endif

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
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

    RUN(testProbeReadsArgumentRegisters());
    RUN(testProbeWritesArgumentRegisters());
    RUN(testProbePreservesGPRS());
    RUN(testProbeModifiesStackPointerToInsideProbeStateOnStack());
    RUN(testProbeModifiesStackPointerToNBytesBelowSP());
    RUN(testProbeModifiesProgramCounter());
    RUN(testProbeModifiesStackValues());

    RUN(testByteSwap());
    RUN(testMoveDoubleConditionally32());
    RUN(testMoveDoubleConditionally64());
    RUN(testLoadBaseIndex());
    RUN(testStoreBaseIndex());

    RUN(testCagePreservesPACFailureBit());

    RUN(testBranchIfType());
    RUN(testBranchIfNotType());

    RUN(testOrImmMem());

    RUN(testAndOrDouble());

    RUN(testGPRInfoConsistency());

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
                            Locker locker { lock };
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
