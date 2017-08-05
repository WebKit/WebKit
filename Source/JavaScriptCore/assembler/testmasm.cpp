/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Threading.h>

// We don't have a NO_RETURN_DUE_TO_EXIT, nor should we. That's ridiculous.
static bool hiddenTruthBecauseNoReturnIsStupid() { return true; }

static void usage()
{
    dataLog("Usage: testmasm [<filter>]\n");
    if (hiddenTruthBecauseNoReturnIsStupid())
        exit(1);
}

#if ENABLE(JIT)

using namespace JSC;

namespace {

StaticLock crashLock;

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
#define testDoubleWord(x) static_cast<double>(testWord(x))

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK(x) do {                                                   \
        if (!!(x))                                                      \
            break;                                                      \
        crashLock.lock();                                               \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #x); \
        CRASH();                                                        \
    } while (false)

#define CHECK_DOUBLE_BITWISE_EQ(a, b) \
    CHECK(bitwise_cast<uint64_t>(a) == bitwise_cast<uint64_t>(a))

#if ENABLE(MASM_PROBE)
bool isPC(MacroAssembler::RegisterID id)
{
#if CPU(ARM_THUMB2) || CPU(ARM_TRADITIONAL)
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
#endif
    return false;
}
#endif // ENABLE(MASM_PROBE)

MacroAssemblerCodeRef compile(Generator&& generate)
{
    CCallHelpers jit;
    generate(jit);
    LinkBuffer linkBuffer(jit, nullptr);
    return FINALIZE_CODE(linkBuffer, ("testmasm compilation"));
}

template<typename T, typename... Arguments>
T invoke(MacroAssemblerCodeRef code, Arguments... arguments)
{
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(code.code().executableAddress());
    return function(arguments...);
}

template<typename T, typename... Arguments>
T compileAndRun(Generator&& generator, Arguments... arguments)
{
    return invoke<T>(compile(WTFMove(generator)), arguments...);
}

void testSimple()
{
    CHECK(compileAndRun<int>([] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();
        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    }) == 42);
}

#if ENABLE(MASM_PROBE)
void testProbeReadsArgumentRegisters()
{
    bool probeWasCalled = false;
    compileAndRun<void>([&] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();

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

        jit.probe([&] (ProbeContext* context) {
            probeWasCalled = true;
            CHECK(context->gpr(GPRInfo::argumentGPR0) == testWord(0));
            CHECK(context->gpr(GPRInfo::argumentGPR1) == testWord(1));
            CHECK(context->gpr(GPRInfo::argumentGPR2) == testWord(2));
            CHECK(context->gpr(GPRInfo::argumentGPR3) == testWord(3));

            CHECK_DOUBLE_BITWISE_EQ(context->fpr(FPRInfo::fpRegT0), static_cast<double>(testWord32(0)));
            CHECK_DOUBLE_BITWISE_EQ(context->fpr(FPRInfo::fpRegT1),  static_cast<double>(testWord32(1)));
        });
        jit.emitFunctionEpilogue();
        jit.ret();
    });
    CHECK(probeWasCalled);
}

void testProbeWritesArgumentRegisters()
{
    // This test relies on testProbeReadsArgumentRegisters() having already validated
    // that we can read from argument registers. We'll use that ability to validate
    // that our writes did take effect.
    unsigned probeCallCount = 0;
    compileAndRun<void>([&] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();

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
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            context->gpr(GPRInfo::argumentGPR0) = testWord(0);
            context->gpr(GPRInfo::argumentGPR1) = testWord(1);
            context->gpr(GPRInfo::argumentGPR2) = testWord(2);
            context->gpr(GPRInfo::argumentGPR3) = testWord(3);
            
            context->fpr(FPRInfo::fpRegT0) = testWord32(0);
            context->fpr(FPRInfo::fpRegT1) = testWord32(1);
        });

        // Validate that expected values were written.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            CHECK(context->gpr(GPRInfo::argumentGPR0) == testWord(0));
            CHECK(context->gpr(GPRInfo::argumentGPR1) == testWord(1));
            CHECK(context->gpr(GPRInfo::argumentGPR2) == testWord(2));
            CHECK(context->gpr(GPRInfo::argumentGPR3) == testWord(3));

            CHECK_DOUBLE_BITWISE_EQ(context->fpr(FPRInfo::fpRegT0), static_cast<double>(testWord32(0)));
            CHECK_DOUBLE_BITWISE_EQ(context->fpr(FPRInfo::fpRegT1), static_cast<double>(testWord32(1)));
        });

        jit.emitFunctionEpilogue();
        jit.ret();
    });
    CHECK(probeCallCount == 2);
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
    MacroAssembler::CPUState originalState;

    compileAndRun<void>([&] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();

        // Write expected values into the registers (except for sp, fp, and pc).
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                originalState.gpr(id) = context->gpr(id);
                if (isSpecialGPR(id))
                    continue;
                context->gpr(id) = testWord(static_cast<int>(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
                originalState.fpr(id) = context->fpr(id);
                context->fpr(id) = testDoubleWord(id);
            }
        });

        // Invoke the probe to call a lot of functions and trash register values.
        jit.probe([&] (ProbeContext*) {
            probeCallCount++;
            CHECK(testFunctionToTrashGPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9) == 10);
            CHECK(testFunctionToTrashFPRs(0, 1, 2, 3, 4, 5, 6, 7, 8, 9) == 10);
        });

        // Validate that the registers have the expected values.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSP(id) || isFP(id)) {
                    CHECK(context->gpr(id) == originalState.gpr(id));
                    continue;
                }
                if (isSpecialGPR(id))
                    continue;
                CHECK(context->gpr(id) == testWord(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                CHECK_DOUBLE_BITWISE_EQ(context->fpr(id), testDoubleWord(id));
        });

        // Restore the original state.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                context->gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                context->fpr(id) = originalState.fpr(id);
        });

        // Validate that the original state was restored.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                CHECK(context->gpr(id) == originalState.gpr(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                CHECK_DOUBLE_BITWISE_EQ(context->fpr(id), originalState.fpr(id));
        });

        jit.emitFunctionEpilogue();
        jit.ret();
    });
    CHECK(probeCallCount == 5);
}

void testProbeModifiesStackPointer(WTF::Function<void*(ProbeContext*)> computeModifiedStack)
{
    unsigned probeCallCount = 0;
    MacroAssembler::CPUState originalState;
    uint8_t* originalSP { nullptr };
    void* modifiedSP { nullptr };
    uintptr_t modifiedFlags { 0 };
    
#if CPU(X86) || CPU(X86_64)
    auto flagsSPR = X86Registers::eflags;
    uintptr_t flagsMask = 0xc5;
#elif CPU(ARM_THUMB2) || CPU(ARM_TRADITIONAL)
    auto flagsSPR = ARMRegisters::apsr;
    uintptr_t flagsMask = 0xf0000000;
#elif CPU(ARM64)
    auto flagsSPR = ARM64Registers::nzcv;
    uintptr_t flagsMask = 0xf0000000;
#endif

    compileAndRun<void>([&] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();

        // Preserve original stack pointer and modify the sp, and
        // write expected values into other registers (except for fp, and pc).
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                originalState.gpr(id) = context->gpr(id);
                if (isSpecialGPR(id))
                    continue;
                context->gpr(id) = testWord(static_cast<int>(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id)) {
                originalState.fpr(id) = context->fpr(id);
                context->fpr(id) = testWord(id);
            }

            originalState.spr(flagsSPR) = context->spr(flagsSPR);
            modifiedFlags = originalState.spr(flagsSPR) ^ flagsMask;
            context->spr(flagsSPR) = modifiedFlags;

            originalSP = reinterpret_cast<uint8_t*>(context->sp());
            modifiedSP = computeModifiedStack(context);
            context->sp() = modifiedSP;
        });

        // Validate that the registers have the expected values.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isFP(id)) {
                    CHECK(context->gpr(id) == originalState.gpr(id));
                    continue;
                }
                if (isSpecialGPR(id))
                    continue;
                CHECK(context->gpr(id) == testWord(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                CHECK_DOUBLE_BITWISE_EQ(context->fpr(id), testDoubleWord(id));
            CHECK(context->spr(flagsSPR) == modifiedFlags);
            CHECK(context->sp() == modifiedSP);
        });

        // Restore the original state.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                context->gpr(id) = originalState.gpr(id);
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                context->fpr(id) = originalState.fpr(id);
            context->spr(flagsSPR) = originalState.spr(flagsSPR);
            context->sp() = originalSP;
        });

        // Validate that the original state was restored.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            for (auto id = CCallHelpers::firstRegister(); id <= CCallHelpers::lastRegister(); id = nextID(id)) {
                if (isSpecialGPR(id))
                    continue;
                CHECK(context->gpr(id) == originalState.gpr(id));
            }
            for (auto id = CCallHelpers::firstFPRegister(); id <= CCallHelpers::lastFPRegister(); id = nextID(id))
                CHECK_DOUBLE_BITWISE_EQ(context->fpr(id),  originalState.fpr(id));
            CHECK(context->spr(flagsSPR) == originalState.spr(flagsSPR));
            CHECK(context->sp() == originalSP);
        });

        jit.emitFunctionEpilogue();
        jit.ret();
    });
    CHECK(probeCallCount == 4);
}

void testProbeModifiesStackPointerToInsideProbeContextOnStack()
{
    size_t increment = sizeof(uintptr_t);
#if CPU(ARM64)
    // The ARM64 probe uses ldp and stp which require 16 byte alignment.
    increment = 2 * sizeof(uintptr_t);
#endif
    for (size_t offset = 0; offset < sizeof(ProbeContext); offset += increment) {
        testProbeModifiesStackPointer([=] (ProbeContext* context) -> void* {
            return reinterpret_cast<uint8_t*>(context) + offset;
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
        testProbeModifiesStackPointer([=] (ProbeContext* context) -> void* {
            return reinterpret_cast<uint8_t*>(context->cpu.sp()) - offset;
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

    MacroAssemblerCodeRef continuation = compile([&] (CCallHelpers& jit) {
        // Validate that we reached the continuation.
        jit.probe([&] (ProbeContext*) {
            probeCallCount++;
            continuationWasReached = true;
        });

        jit.emitFunctionEpilogue();
        jit.ret();
    });

    compileAndRun<void>([&] (CCallHelpers& jit) {
        jit.emitFunctionPrologue();

        // Write expected values into the registers.
        jit.probe([&] (ProbeContext* context) {
            probeCallCount++;
            context->pc() = continuation.code().executableAddress();
        });

        jit.breakpoint(); // We should never get here.
    });
    CHECK(probeCallCount == 2);
    CHECK(continuationWasReached);
}
#endif // ENABLE(MASM_PROBE)

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

void run(const char* filter)
{
    JSC::initializeThreading();

    Deque<RefPtr<SharedTask<void()>>> tasks;

    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || !!strcasestr(testName, filter);
    };

    RUN(testSimple());

#if ENABLE(MASM_PROBE)
    RUN(testProbeReadsArgumentRegisters());
    RUN(testProbeWritesArgumentRegisters());
    RUN(testProbePreservesGPRS());
    RUN(testProbeModifiesStackPointerToInsideProbeContextOnStack());
    RUN(testProbeModifiesStackPointerToNBytesBelowSP());
    RUN(testProbeModifiesProgramCounter());
#endif

    if (tasks.isEmpty())
        usage();

    Lock lock;

    Vector<RefPtr<Thread>> threads;
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

    for (RefPtr<Thread> thread : threads)
        thread->waitForCompletion();
    crashLock.lock();
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
