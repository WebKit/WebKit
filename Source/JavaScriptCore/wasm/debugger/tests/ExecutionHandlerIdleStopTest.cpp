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
#include "ExecutionHandlerIdleStopTest.h"

#include <wtf/DataLog.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "Completion.h"
#include "ExecutionHandlerTestSupport.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "Protect.h"
#include "SourceCode.h"
#include "SourceOrigin.h"
#include "StructureInlines.h"
#include "VM.h"
#include "VMManager.h"
#include "WasmDebugServer.h"
#include "WasmExecutionHandler.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace ExecutionHandlerIdleStopTest {

using ExecutionHandlerTestSupport::setupTestEnvironment;
using JSC::VM;
using JSC::VMManager;
using JSC::Wasm::DebugServer;
using JSC::Wasm::ExecutionHandler;

// ========== Test Configuration ==========

static constexpr bool verboseLogging = false;
static constexpr unsigned STRESS_TEST_ITERATIONS = 1000;
static constexpr ASCIILiteral WORKER_THREAD_NAME = "RunLoopDispatchTestVM"_s;

// ========== Test Runtime State ==========

static int failuresFound = 0;
static DebugServer* debugServer = nullptr;
static ExecutionHandler* executionHandler = nullptr;
static std::atomic<bool> doneTesting = false;

#define TEST_LOG(...) dataLogLn(__VA_ARGS__)
#define VLOG(...) dataLogLnIf(verboseLogging, __VA_ARGS__)

#define CHECK(condition, ...)                                   \
    do {                                                        \
        if (!(condition)) {                                     \
            dataLogLn("FAIL: ", #condition, ": ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__);        \
            failuresFound++;                                    \
            return;                                             \
        }                                                       \
    } while (false)

// ========== SIMPLE VM TASK ==========

// Control when VM starts executing (becomes active)
static std::atomic<bool> runVM = false;

// Signaling for VM construction completion
static Lock vmReadyLock;
static Condition vmReadyCondition;
static unsigned vmReadyCount = 0;

static void simpleVMTask()
{
    VLOG("[VMThread] Starting VM construction");
    // Create VM once - RunLoop dispatch will handle both active and idle states
    VM& vm = VM::create(JSC::HeapType::Large).leakRef();
    JSC::JSGlobalObject* globalObject = nullptr;

    {
        JSC::JSLockHolder locker(vm);
        globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
        gcProtect(globalObject);

        // Signal that VM is fully constructed and ready
        {
            Locker locker { vmReadyLock };
            vmReadyCount++;
            vmReadyCondition.notifyAll();
        }
        VLOG("[VMThread] VM constructed and ready");
    } // Release API lock - VM is now truly idle without lock

    // Keep VM alive and execute script when signaled
    while (!doneTesting.load()) {
        VLOG("[VMThread] Top of loop, runVM=", runVM.load(), ", doneTesting=", doneTesting.load());

        // Wait for signal to execute (exchange atomically reads and resets flag)
        // Process RunLoop events while idle to handle dispatch callbacks
        // IMPORTANT: API lock is NOT held here - VM is truly idle
        while (!runVM.exchange(false) && !doneTesting.load())
            WTF::RunLoop::cycle(DefaultRunLoopMode);

        VLOG("[VMThread] After wait loop, doneTesting=", doneTesting.load());
        if (doneTesting.load()) {
            VLOG("[VMThread] doneTesting detected, breaking loop");
            break;
        }

        VLOG("[VMThread] About to execute script");
        // Execute simple script - VM becomes active (calls notifyVMActivation)
        {
            JSC::JSLockHolder locker(vm);
            JSC::SourceOrigin origin(URL({ }, "test"_s));
            JSC::SourceCode sourceCode = JSC::makeSource("1 + 1"_s, origin, JSC::SourceTaintedOrigin::Untainted);

            NakedPtr<JSC::Exception> exception;
            JSC::evaluate(globalObject, sourceCode, JSC::JSValue(), exception);
            VLOG("[VMThread] Script execution completed");
        } // Release API lock - VM becomes idle again

        // Script finished - VM becomes idle again
    }

    // Manually release the VM reference to trigger destructor
    {
        JSC::JSLockHolder locker(vm);
        gcUnprotect(globalObject);
        vm.derefSuppressingSaferCPPChecking();
    }

    VLOG("[VMThread] Exiting simpleVMTask");
}

// ========== HELPER FUNCTIONS ==========

static void waitForVMsConstruction(unsigned count)
{
    Locker locker { vmReadyLock };
    while (vmReadyCount < count)
        vmReadyCondition.wait(vmReadyLock);
}

static void waitForVMCleanup()
{
    VLOG("Waiting for VM from previous test to be destroyed...");
    bool cleanedUp = ExecutionHandlerTestSupport::waitForCondition([]() {
        return !VMManager::info().numberOfVMs;
    });

    CHECK(cleanedUp, "VM not cleaned up within timeout (count: ", VMManager::info().numberOfVMs, ")");
    VLOG("VM cleaned up successfully");
}

static bool isRunning()
{
    auto info = VMManager::info();
    return info.worldMode == VMManager::Mode::RunAll;
}

static bool isStopped()
{
    auto info = VMManager::info();
    return info.worldMode == VMManager::Mode::Stopped;
}

// ========== ORDERING 1: VM Enter → Interrupt → Continue ==========
// VM becomes active, then gets interrupted

static void testOrderingVMEnterInterruptContinue()
{
    TEST_LOG("\n=== Ordering: VM Enter → Interrupt → Continue ===");
    TEST_LOG("VM signaled to run, then interrupted");

    // Ensure no VMs from previous tests
    auto initialInfo = VMManager::info();
    CHECK(!initialInfo.numberOfVMs, "Expected 0 VMs at test start, got ", initialInfo.numberOfVMs);

    // Create ONE VM thread that will be reused for all iterations
    runVM = false;
    vmReadyCount = 0;
    RefPtr<Thread> vmThread = Thread::create(WORKER_THREAD_NAME, simpleVMTask);

    // Wait for VM to be fully constructed and idle
    waitForVMsConstruction(1);

    // Verify exactly 1 VM now exists
    auto afterInfo = VMManager::info();
    CHECK(afterInfo.numberOfVMs == 1, "Expected 1 VM after construction, got ", afterInfo.numberOfVMs);

    unsigned successCount = 0;

    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("[Test1][Iter ", i, "] start >>>>>>>>>>>>>>>>>>>>>>> ");

        // Signal VM to execute (becomes active)
        runVM = true;

        // Interrupt - may catch VM while active or before it starts
        executionHandler->interrupt();

        // Verify we got a stop (either trap or RunLoop dispatch callback)
        CHECK(isStopped(), "Should be stopped after interrupt");
        auto info = VMManager::info();
        VLOG("[Test1][Iter ", i, "] After interrupt: worldMode=", (int)info.worldMode, ", numberOfVMs=", info.numberOfVMs, ", numberOfActiveVMs=", info.numberOfActiveVMs);

        // Continue
        executionHandler->resume();

        // Verify world is running
        CHECK(isRunning(), "Should be running after resume");

        successCount++;
        VLOG("[Test1][Iter ", i, "] end <<<<<<<<<<<<<<<<<<<<<<<<< ");
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    // Cleanup - MUST resume world first to release any waiting VMs
    VMManager::requestResumeAll(VMManager::StopReason::WasmDebugger);
    doneTesting = true;
    runVM = true;
    vmThread->waitForCompletion();
    waitForVMCleanup();
    executionHandler->reset();
    doneTesting = false;
}

// ========== ORDERING 2: Interrupt → VM Enter → Continue ==========
// Interrupt while idle, VM becomes active during stop

static void testOrderingInterruptVMEnterContinue()
{
    TEST_LOG("\n=== Ordering: Interrupt → VM Enter → Continue ===");
    TEST_LOG("VM enters at various points during interrupt");

    // Ensure no VMs from previous tests
    auto initialInfo = VMManager::info();
    CHECK(!initialInfo.numberOfVMs, "Expected 0 VMs at test start, got ", initialInfo.numberOfVMs);

    // Create ONE VM thread that will be reused for all iterations
    runVM = false;
    vmReadyCount = 0;
    RefPtr<Thread> vmThread = Thread::create(WORKER_THREAD_NAME, simpleVMTask);

    // Wait for VM to be fully constructed and idle
    waitForVMsConstruction(1);

    // Verify exactly 1 VM now exists
    auto afterInfo = VMManager::info();
    CHECK(afterInfo.numberOfVMs == 1, "Expected 1 VM after construction, got ", afterInfo.numberOfVMs);

    unsigned successCount = 0;

    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("[Test2][Iter ", i, "] start >>>>>>>>>>>>>>>>>>>>>>> ");

        // Interrupt FIRST (VM idle, not executing)
        // RunLoop dispatch will handle callback delivery
        executionHandler->interrupt();

        // Verify we got a stop (via RunLoop dispatch since VM was idle)
        CHECK(isStopped(), "Should be stopped after interrupt");
        auto info = VMManager::info();
        VLOG("[Test2][Iter ", i, "] After interrupt: worldMode=", (int)info.worldMode);

        // Signal VM to start executing (natural timing creates races)
        runVM = true;
        VLOG("[Test2][Iter ", i, "] Signaled VM to run");

        // Continue (VM may become active before, during, or after this call)
        executionHandler->resume();
        VLOG("[Test2][Iter ", i, "] After resume");

        // Verify resume completed correctly
        CHECK(isRunning(), "Should be running after resume");

        successCount++;
        VLOG("[Test2][Iter ", i, "] end <<<<<<<<<<<<<<<<<<<<<<<<< ");
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    // Cleanup - MUST resume world first to release any waiting VMs
    VMManager::requestResumeAll(VMManager::StopReason::WasmDebugger);
    doneTesting = true;
    runVM = true;
    vmThread->waitForCompletion();
    waitForVMCleanup();
    executionHandler->reset();
    doneTesting = false;
}

// ========== ORDERING 3: Interrupt → Continue → VM Enter ==========
// VM enters after resume completes

static void testOrderingInterruptContinueVMEnter()
{
    TEST_LOG("\n=== Ordering: Interrupt → Continue → VM Enter ===");
    TEST_LOG("VM enters after resume completes");

    // Ensure no VMs from previous tests
    auto initialInfo = VMManager::info();
    CHECK(!initialInfo.numberOfVMs, "Expected 0 VMs at test start, got ", initialInfo.numberOfVMs);

    // Create ONE VM thread that will be reused for all iterations
    runVM = false;
    vmReadyCount = 0;
    RefPtr<Thread> vmThread = Thread::create(WORKER_THREAD_NAME, simpleVMTask);

    // Wait for VM to be fully constructed and idle
    waitForVMsConstruction(1);

    // Verify exactly 1 VM now exists
    auto afterInfo = VMManager::info();
    CHECK(afterInfo.numberOfVMs == 1, "Expected 1 VM after construction, got ", afterInfo.numberOfVMs);

    unsigned successCount = 0;

    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("[Test3][Iter ", i, "] start >>>>>>>>>>>>>>>>>>>>>>> ");

        // Interrupt FIRST (VM should be idle)
        executionHandler->interrupt();

        // Verify we got a stop
        CHECK(isStopped(), "Should be stopped after interrupt");
        auto info = VMManager::info();
        VLOG("[Test3][Iter ", i, "] After interrupt: worldMode=", (int)info.worldMode);

        // Continue BEFORE VM starts executing
        executionHandler->resume();
        VLOG("[Test3][Iter ", i, "] After resume");

        // Verify world is running
        CHECK(isRunning(), "Should be running after resume");

        // Signal VM to start executing AFTER resume
        runVM = true;
        VLOG("[Test3][Iter ", i, "] Signaled VM to run");

        // VM should be running normally (not stopped)
        info = VMManager::info();
        CHECK(info.worldMode == VMManager::Mode::RunAll, "World should remain running");
        CHECK(info.numberOfVMs >= 1, "VM should be running");

        successCount++;
        VLOG("[Test3][Iter ", i, "] end <<<<<<<<<<<<<<<<<<<<<<<<< ");
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    // Cleanup - MUST resume world first to release any waiting VMs
    VMManager::requestResumeAll(VMManager::StopReason::WasmDebugger);
    doneTesting = true;
    runVM = true;
    vmThread->waitForCompletion();
    waitForVMCleanup();
    executionHandler->reset();
    doneTesting = false;
}

// ========== ORDERING 4: Idle VM Interrupt/Resume Loops ==========
// VM stays idle throughout - pure RunLoop dispatch testing

static void testIdleVMInterruptResumeLoops()
{
    TEST_LOG("\n=== Idle VM Interrupt/Resume Loops ===");
    TEST_LOG("VM remains idle, interrupt/resume via RunLoop dispatch only");

    // Ensure no VMs from previous tests
    auto initialInfo = VMManager::info();
    CHECK(!initialInfo.numberOfVMs, "Expected 0 VMs at test start, got ", initialInfo.numberOfVMs);

    // Create ONE VM thread that will remain idle the entire test
    runVM = false;
    vmReadyCount = 0;
    RefPtr<Thread> vmThread = Thread::create(WORKER_THREAD_NAME, simpleVMTask);

    // Wait for VM to be fully constructed and idle
    waitForVMsConstruction(1);

    // Verify exactly 1 VM now exists
    auto afterInfo = VMManager::info();
    CHECK(afterInfo.numberOfVMs == 1, "Expected 1 VM after construction, got ", afterInfo.numberOfVMs);

    unsigned successCount = 0;

    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("[Test4][Iter ", i, "] start >>>>>>>>>>>>>>>>>>>>>>> ");

        // Interrupt while VM is idle - ONLY RunLoop dispatch can handle this
        executionHandler->interrupt();

        // Verify we got a stop (via RunLoop dispatch callback)
        CHECK(isStopped(), "Should be stopped after interrupt");
        auto info = VMManager::info();
        VLOG("[Test4][Iter ", i, "] After interrupt: worldMode=", (int)info.worldMode, ", numberOfVMs=", info.numberOfVMs, ", numberOfActiveVMs=", info.numberOfActiveVMs);

        // Resume
        executionHandler->resume();

        // Verify world is running
        CHECK(isRunning(), "Should be running after resume");

        // VM stays idle - never signal runVM = true
        // This ensures we're testing pure RunLoop dispatch without any trap checking

        successCount++;
        VLOG("[Test4][Iter ", i, "] end <<<<<<<<<<<<<<<<<<<<<<<<< ");
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    // Cleanup
    doneTesting = true;
    runVM = true; // Allow thread to exit
    vmThread->waitForCompletion();
    waitForVMCleanup();
    executionHandler->reset();
    doneTesting = false;
}

// ========== ORDERING 5: Idle VM + Active VM on Thread ==========
// Test idle VM without owner thread + active VM running

static void idleVMTask()
{
    VLOG("[IdleVM] Starting VM construction");
    VM& vm = VM::create(JSC::HeapType::Large).leakRef();
    JSC::JSGlobalObject* globalObject = nullptr;

    {
        JSC::JSLockHolder locker(vm);
        globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
        gcProtect(globalObject);

        // Signal that VM is fully constructed and ready
        {
            Locker locker { vmReadyLock };
            vmReadyCount++;
            vmReadyCondition.notifyAll();
        }
        VLOG("[IdleVM] VM constructed and ready");
    } // Release API lock - VM is now idle

    // Keep VM alive but idle - just cycle RunLoop to process dispatch callbacks
    // Never execute any JavaScript code - truly idle
    while (!doneTesting.load()) {
        WTF::RunLoop::cycle(DefaultRunLoopMode);
        VLOG("[IdleVM] Cycled RunLoop");
    }

    // Cleanup
    {
        JSC::JSLockHolder locker(vm);
        gcUnprotect(globalObject);
        vm.derefSuppressingSaferCPPChecking();
    }
    VLOG("[IdleVM] Exiting");
}

static void activeVMTask()
{
    VLOG("[ActiveVM] Starting VM construction");
    VM& vm = VM::create(JSC::HeapType::Large).leakRef();
    JSC::JSGlobalObject* globalObject = nullptr;

    {
        JSC::JSLockHolder locker(vm);
        globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
        gcProtect(globalObject);

        // Signal that VM is fully constructed and ready
        {
            Locker locker { vmReadyLock };
            vmReadyCount++;
            vmReadyCondition.notifyAll();
        }
        VLOG("[ActiveVM] VM constructed and ready");
    } // Release API lock - allow dispatch callbacks to execute

    // Keep VM alive and run continuous loop
    while (!doneTesting.load()) {
        // Cycle RunLoop to process dispatch callbacks while not holding JSLock
        WTF::RunLoop::cycle(DefaultRunLoopMode);

        // Execute script - VM becomes active
        {
            JSC::JSLockHolder locker(vm);
            JSC::SourceOrigin origin(URL({ }, "active-vm"_s));
            // Tight loop that checks traps frequently
            JSC::SourceCode sourceCode = JSC::makeSource("for (var i = 0; i < 1000000; i++) {}"_s, origin, JSC::SourceTaintedOrigin::Untainted);

            NakedPtr<JSC::Exception> exception;
            JSC::evaluate(globalObject, sourceCode, JSC::JSValue(), exception);
            VLOG("[ActiveVM] Loop iteration completed");
        } // Release API lock after each iteration
    }

    // Cleanup
    {
        JSC::JSLockHolder locker(vm);
        gcUnprotect(globalObject);
        vm.derefSuppressingSaferCPPChecking();
    }
    VLOG("[ActiveVM] Exiting");
}

static void testIdleVMWithActiveVM()
{
    TEST_LOG("\n=== 2 Idle VMs + 3 Active VMs on Threads ===");
    TEST_LOG("Test 2 idle VMs (only cycle RunLoop) + 3 active VMs (execute code)");

    // Ensure no VMs from previous tests
    auto initialInfo = VMManager::info();
    CHECK(!initialInfo.numberOfVMs, "Expected 0 VMs at test start, got ", initialInfo.numberOfVMs);

    vmReadyCount = 0;

    // Create 2 idle VMs on their own threads - they will only cycle RunLoop, never execute code
    RefPtr<Thread> idleThread1 = Thread::create("IdleVM1"_s, idleVMTask);
    RefPtr<Thread> idleThread2 = Thread::create("IdleVM2"_s, idleVMTask);

    // Create 3 active VMs on separate threads - they will execute code continuously
    RefPtr<Thread> activeThread1 = Thread::create("ActiveVM1"_s, activeVMTask);
    RefPtr<Thread> activeThread2 = Thread::create("ActiveVM2"_s, activeVMTask);
    RefPtr<Thread> activeThread3 = Thread::create("ActiveVM3"_s, activeVMTask);

    // Wait for all 5 VMs to be fully constructed
    waitForVMsConstruction(5);

    // Verify exactly 5 VMs now exist (2 idle + 3 active)
    auto afterInfo = VMManager::info();
    CHECK(afterInfo.numberOfVMs == 5, "Expected 5 VMs after construction (2 idle + 3 active), got ", afterInfo.numberOfVMs);

    unsigned successCount = 0;

    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("[Test5][Iter ", i, "] start >>>>>>>>>>>>>>>>>>>>>>> ");

        executionHandler->interrupt();
        CHECK(isStopped(), "Should be stopped after interrupt");

        auto info = VMManager::info();
        VLOG("[Test5][Iter ", i, "] After interrupt: worldMode=", (int)info.worldMode, ", numberOfVMs=", info.numberOfVMs, ", numberOfActiveVMs=", info.numberOfActiveVMs);

        CHECK(info.numberOfActiveVMs <= 5, "Expected 0-5 active VMs, got ", info.numberOfActiveVMs);

        executionHandler->resume();
        CHECK(isRunning(), "Should be running after resume");

        successCount++;
        VLOG("[Test5][Iter ", i, "] end <<<<<<<<<<<<<<<<<<<<<<<<< ");
    }

    TEST_LOG("PASS: ", successCount, "/", STRESS_TEST_ITERATIONS, " iterations succeeded");

    // Cleanup - MUST resume world first to release any waiting VMs
    VMManager::requestResumeAll(VMManager::StopReason::WasmDebugger);
    doneTesting = true;
    idleThread1->waitForCompletion();
    idleThread2->waitForCompletion();
    activeThread1->waitForCompletion();
    activeThread2->waitForCompletion();
    activeThread3->waitForCompletion();

    waitForVMCleanup();
    executionHandler->reset();

    doneTesting = false;
    vmReadyCount = 0;
}

// ========== MAIN TEST RUNNER ==========

UNUSED_FUNCTION static int runIdleVMStopStressTests()
{
    TEST_LOG("========================================");
    TEST_LOG("Idle VM Stress Tests");
    TEST_LOG("Testing Interrupt/Resume Race Scenarios");
    TEST_LOG("========================================");

    setupTestEnvironment(debugServer, executionHandler);

    // Run the 4 core orderings - all should work uniformly with RunLoop dispatch
    testOrderingVMEnterInterruptContinue(); // VM active when interrupted
    testOrderingInterruptVMEnterContinue(); // VM enters during interrupt
    testOrderingInterruptContinueVMEnter(); // VM enters after resume
    testIdleVMInterruptResumeLoops(); // VM stays idle throughout
    testIdleVMWithActiveVM(); // 2 Idle VMs + 3 Active VMs - stress test with multiple VMs

    TEST_LOG("\n========================================");
    TEST_LOG(failuresFound ? "FAIL" : "PASS", " - Idle VM Stress Tests");
    TEST_LOG("Total Failures: ", failuresFound);
    TEST_LOG("========================================");

    return failuresFound;
}

#undef TEST_LOG
#undef CHECK

} // namespace ExecutionHandlerIdleStopTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)

int testExecutionHandlerIdleStop()
{
#if ENABLE(WEBASSEMBLY_DEBUGGER) && CPU(ARM64)
    return ExecutionHandlerIdleStopTest::runIdleVMStopStressTests();
#else
    dataLogLn("Idle VM Stress Tests SKIPPED (only supported on ARM64)");
    return 0;
#endif
}
