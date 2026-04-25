/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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
#include "ExecutionHandlerVMLifecycleTest.h"

#include <wtf/DataLog.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "Completion.h"
#include "ExecutionHandlerTestSupport.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectInlines.h"
#include "JSLock.h"
#include "Protect.h"
#include "SourceCode.h"
#include "SourceOrigin.h"
#include "StructureCreateInlines.h"
#include "VM.h"
#include "VMManager.h"
#include "WasmDebugServer.h"
#include "WasmExecutionHandler.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace ExecutionHandlerVMLifecycleTest {

using ExecutionHandlerTestSupport::setupTestEnvironment;
using ExecutionHandlerTestSupport::waitForCondition;
using JSC::VM;
using JSC::VMManager;
using JSC::Wasm::DebugServer;
using JSC::Wasm::ExecutionHandler;

// ========== Test Configuration ==========

static constexpr bool verboseLogging = false;
// Each test keeps one permanent anchor VM alive so interrupt() always has at least one
// VM to drive the STW to completion.
static constexpr unsigned NUM_VMS = 10;
static constexpr unsigned STRESS_TEST_ITERATIONS = 1000;
static constexpr ASCIILiteral LIFECYCLE_THREAD_NAME = "LifecycleTestVM"_s;

// ========== Test Runtime State ==========

static int failuresFound = 0;
static DebugServer* debugServer = nullptr;
static ExecutionHandler* executionHandler = nullptr;

// doneTesting: signals vmAnchorTask and vmFullLifecycleTask threads to exit.
static std::atomic<bool> doneTesting { false };

// Barrier for waiting until N VM threads have completed construction.
static Lock vmReadyLock;
static Condition vmReadyCondition;
static unsigned vmReadyCount = 0;

#define TEST_LOG(...) dataLogLn(__VA_ARGS__)
#define VLOG(...) dataLogLnIf(verboseLogging, __VA_ARGS__)

// On failure: log, increment counter, and return from the enclosing function.
#define CHECK(condition, ...)                                   \
    do {                                                        \
        if (!(condition)) {                                     \
            dataLogLn("FAIL: ", #condition, ": ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__ ":", __LINE__);         \
            failuresFound++;                                    \
            return;                                             \
        }                                                       \
    } while (false)

// ========== Helpers ==========

static bool isStopped()
{
    return VMManager::info().worldMode == VMManager::Mode::Stopped;
}

static bool isRunning()
{
    return VMManager::info().worldMode == VMManager::Mode::RunAll;
}

static void signalVMReady()
{
    Locker locker { vmReadyLock };
    vmReadyCount++;
    vmReadyCondition.notifyAll();
}

static void waitForVMsConstruction(unsigned count)
{
    Locker locker { vmReadyLock };
    while (vmReadyCount < count)
        vmReadyCondition.wait(vmReadyLock);
}

static void waitForAllVMsGone()
{
    VLOG("Waiting for all VMs (including anchor) to be destroyed...");
    bool ok = waitForCondition([]() {
        return !VMManager::info().numberOfVMs;
    });
    if (!ok) {
        dataLogLn("FAIL: VMs not cleaned up within timeout (count: ", VMManager::info().numberOfVMs, ")");
        failuresFound++;
    }
}

// ========== VM Task: anchor — idles in RunLoop until doneTesting ==========

static void vmAnchorTask()
{
    VLOG("[AnchorVM] Creating VM");
    VM& vm = VM::create(JSC::HeapType::Small).leakRef();
    JSC::JSGlobalObject* globalObject = nullptr;

    {
        JSC::JSLockHolder locker(vm);
        globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
        gcProtect(globalObject);
        signalVMReady();
        VLOG("[AnchorVM] Ready");
    } // Release API lock — VM is idle

    while (!doneTesting.load())
        WTF::RunLoop::cycle(DefaultRunLoopMode);

    VLOG("[AnchorVM] Releasing VM");
    {
        JSC::JSLockHolder locker(vm);
        gcUnprotect(globalObject);
        vm.derefSuppressingSaferCPPChecking();
    }
}

// ========== VM Task: continuous construct → run JS → destroy loop ==========

static void vmFullLifecycleTask()
{
    // Number of RunLoop cycles to spin between JSLock acquisitions. Wider idle windows
    // give STW interrupts a better chance of landing while the VM is registered but not entered.
    static constexpr unsigned idleSpinCount = 3;

    VLOG("[FullLifecycleVM] Starting");
    while (!doneTesting.load()) {
        VM& vm = VM::create(JSC::HeapType::Small).leakRef();
        JSC::JSGlobalObject* globalObject = nullptr;

        for (unsigned i = 0; i < idleSpinCount; ++i)
            WTF::RunLoop::cycle(DefaultRunLoopMode);

        {
            JSC::JSLockHolder locker(vm);
            globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
            gcProtect(globalObject);
        } // Release API lock — VM is idle and in the VMManager list

        for (unsigned i = 0; i < idleSpinCount; ++i)
            WTF::RunLoop::cycle(DefaultRunLoopMode);

        // Execute a trivial script so the VM transitions through active state.
        {
            JSC::JSLockHolder locker(vm);
            JSC::SourceOrigin origin(URL({ }, "lifecycle-test"_s));
            JSC::SourceCode sourceCode = JSC::makeSource("(function() { var x = 0; for (var i = 0; i < 100000; ++i) x += i; return x; })()"_s, origin, JSC::SourceTaintedOrigin::Untainted);
            NakedPtr<JSC::Exception> exception;
            JSC::evaluate(globalObject, sourceCode, JSC::JSValue(), exception);
        }

        for (unsigned i = 0; i < idleSpinCount; ++i)
            WTF::RunLoop::cycle(DefaultRunLoopMode);

        // Destroy — removes VM from the VMManager list.
        {
            JSC::JSLockHolder locker(vm);
            gcUnprotect(globalObject);
            vm.derefSuppressingSaferCPPChecking();
        }
    }
    VLOG("[FullLifecycleVM] Exiting");
}

// ========== Full Lifecycle Race ==========
// NUM_VMS threads cycle construct → run JS → destroy while STW fires concurrently.

static void testFullLifecycleRace()
{
    TEST_LOG("\n=== VM Lifecycle Test: Full Lifecycle Race ===");
    TEST_LOG("1 anchor VM + ", NUM_VMS, " cycling VMs, STW fires concurrently");

    // Start the anchor VM and wait until it is in the VMManager list.
    doneTesting = false;
    vmReadyCount = 0;
    RefPtr<Thread> anchorThread = Thread::create(LIFECYCLE_THREAD_NAME, vmAnchorTask);
    waitForVMsConstruction(1);

    // Cycling VMs: continuously construct → run JS → destroy until doneTesting.
    Vector<RefPtr<Thread>> cyclingThreads;
    cyclingThreads.reserveInitialCapacity(NUM_VMS);
    for (unsigned i = 0; i < NUM_VMS; ++i)
        cyclingThreads.append(Thread::create(LIFECYCLE_THREAD_NAME, vmFullLifecycleTask));

    unsigned successCount = 0;

    for (unsigned iter = 0; iter < STRESS_TEST_ITERATIONS; ++iter) {
        VLOG("[FullLifecycle][Iter ", iter, "] start");

        executionHandler->interrupt();
        CHECK(isStopped(), "Should be stopped after interrupt (full lifecycle iter ", iter, ")");
        VLOG("[FullLifecycle][Iter ", iter, "] stopped OK, numberOfVMs=", VMManager::info().numberOfVMs);

        executionHandler->resume();
        CHECK(isRunning(), "Should be running after resume (full lifecycle iter ", iter, ")");
        VLOG("[FullLifecycle][Iter ", iter, "] resumed OK");

        successCount++;
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    doneTesting = true;
    anchorThread->waitForCompletion();
    for (auto& t : cyclingThreads)
        t->waitForCompletion();
    waitForAllVMsGone();
    executionHandler->reset();
    doneTesting = false;
}

// ========== Main Test Runner ==========

UNUSED_FUNCTION static int runVMLifecycleTests()
{
    TEST_LOG("========================================");
    TEST_LOG("VM Lifecycle Race Tests");
    TEST_LOG("Validates debugState() lifetime matches VMManager list membership");
    TEST_LOG("Each test: 1 anchor VM + ", NUM_VMS, " test VMs  STRESS_TEST_ITERATIONS=", STRESS_TEST_ITERATIONS);
    TEST_LOG("========================================");

    setupTestEnvironment(debugServer, executionHandler);

    testFullLifecycleRace();

    TEST_LOG("\n========================================");
    TEST_LOG(failuresFound ? "FAIL" : "PASS", " - VM Lifecycle Race Tests");
    TEST_LOG("Total Failures: ", failuresFound);
    TEST_LOG("========================================");

    return failuresFound;
}

#undef TEST_LOG
#undef VLOG
#undef CHECK

} // namespace ExecutionHandlerVMLifecycleTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)

int testExecutionHandlerVMLifecycle()
{
#if ENABLE(WEBASSEMBLY_DEBUGGER) && CPU(ARM64)
    return ExecutionHandlerVMLifecycleTest::runVMLifecycleTests();
#else
    dataLogLn("VM Lifecycle Race Tests SKIPPED (only supported on ARM64)");
    return 0;
#endif
}
