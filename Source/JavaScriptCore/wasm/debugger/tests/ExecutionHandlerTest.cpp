/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExecutionHandlerTest.h"

#if ENABLE(WEBASSEMBLY) && ENABLE(REMOTE_INSPECTOR)

#include "ExecutionHandlerTestSupport.h"
#include "JSWebAssemblyInstance.h"
#include "TestScripts.h"
#include "VM.h"
#include "VMManager.h"
#include "WasmBreakpointManager.h"
#include "WasmCalleeGroup.h"
#include "WasmDebugServer.h"
#include "WasmDebugServerUtilities.h"
#include "WasmExecutionHandler.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmModuleManager.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace ExecutionHandlerTest {

using ExecutionHandlerTestSupport::defaultTimeoutSeconds;
using ExecutionHandlerTestSupport::setupTestEnvironment;
using ExecutionHandlerTestSupport::verboseLogging;
using ExecutionHandlerTestSupport::waitForCondition;
using ExecutionHandlerTestSupport::workerThreadTask;
using JSC::JSWebAssemblyInstance;
using JSC::VM;
using JSC::VMManager;
using JSC::Wasm::Breakpoint;
using JSC::Wasm::CalleeGroup;
using JSC::Wasm::DebugServer;
using JSC::Wasm::DebugState;
using JSC::Wasm::ExecutionHandler;
using JSC::Wasm::FunctionSpaceIndex;
using JSC::Wasm::ModuleManager;
using TestScripts::TestScript;

// ========== Test runtime state ==========

static constexpr unsigned RAPID_CYCLES_COUNT = 1000;
static constexpr unsigned CONTEXT_SWITCH_MULTIPLIER = 1000;
static constexpr unsigned BREAKPOINT_CONTINUE_CYCLES_COUNT = 1000;
static constexpr unsigned SINGLE_STEPPING_CYCLES_COUNT = 1000;
static constexpr ASCIILiteral WORKER_THREAD_NAME = "WasmStressTest"_s;

static int failuresFound = 0;
static DebugServer* debugServer = nullptr;
static ExecutionHandler* executionHandler = nullptr;
static const TestScript* currentScript = nullptr;
bool doneTesting = false;

#define VLOG(...) dataLogLnIf(verboseLogging, __VA_ARGS__)
#define TEST_LOG(...) dataLogLn(__VA_ARGS__)

#define CHECK(condition, ...) \
    do { \
        if (!(condition)) { \
            dataLogLn("FAIL: ", #condition, ": ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__); \
            CRASH(); \
        } \
    } while (false)

static void waitForConditionAndCheck(const char* errorMessage, std::function<bool()> predicate)
{
    bool result = waitForCondition(predicate);
    CHECK(result, errorMessage);
}

// ========== HELPER FUNCTIONS ==========

static void interruptAndWaitForStop(const char* errorMessage)
{
    VLOG("Interrupting VMs...");
    executionHandler->interrupt();
    waitForConditionAndCheck(errorMessage, [&]() {
        auto info = VMManager::info();
        return info.worldMode == VMManager::Mode::Stopped && info.targetVM->debugState()->isStopped();
    });
}

static unsigned setBreakpointsAtAllFunctionEntries(Breakpoint::Type type)
{
    VLOG("Setting breakpoints at all function entries...");
    unsigned breakpointsSet = 0;

    ModuleManager& moduleManager = debugServer->moduleManager();
    uint32_t maxInstanceId = moduleManager.nextInstanceId();

    for (uint32_t instanceId = 0; instanceId < maxInstanceId; ++instanceId) {
        JSWebAssemblyInstance* instance = moduleManager.jsInstance(instanceId);
        if (!instance)
            continue;

        auto& module = instance->module();
        auto& moduleInfo = module.moduleInformation();
        uint32_t internalCount = moduleInfo.internalFunctionCount();

        VLOG("  Instance ", instanceId, ": ", internalCount, " functions");

        for (uint32_t funcIndex = 0; funcIndex < internalCount; ++funcIndex) {
            FunctionSpaceIndex spaceIndex = moduleInfo.toSpaceIndex(JSC::Wasm::FunctionCodeIndex(funcIndex));
            auto callee = instance->calleeGroup()->ipintCalleeFromFunctionIndexSpace(spaceIndex);
            executionHandler->setBreakpointAtEntry(instance, callee.ptr(), type);
            breakpointsSet++;
        }
    }

    VLOG("Set ", breakpointsSet, " breakpoints total");
    return breakpointsSet;
}

static void clearBreakpointsAndResume(const char* errorMessage)
{
    executionHandler->breakpointManager()->clearAllBreakpoints();
    executionHandler->resume();
    waitForConditionAndCheck(errorMessage, [&]() {
        return VMManager::info().worldMode == VMManager::Mode::RunAll;
    });
}

// ========== BASIC TESTS ==========

static void testInterruptAndResume()
{
    TEST_LOG("\n=== Interrupt and Resume ===");

    VLOG("Initial state: ", VMManager::info());

    interruptAndWaitForStop("VMs did not stop after interrupt");

    VLOG("After interrupt: ", VMManager::info());
    CHECK(executionHandler->vm(), "Should have target VM");

    VLOG("Calling resume()...");
    executionHandler->resume();

    waitForConditionAndCheck("VMs did not resume", [&]() {
        return VMManager::info().worldMode == VMManager::Mode::RunAll;
    });
    VLOG("After resume: ", VMManager::info());

    TEST_LOG(!failuresFound ? "PASS" : "FAIL");
}

static void testRapidInterruptResumeCycles()
{
    TEST_LOG("\n=== Rapid Interrupt/Resume Cycles ===");

    int initialFailures = failuresFound;

    for (unsigned i = 0; i < RAPID_CYCLES_COUNT; ++i) {
        VLOG("Cycle ", i);

        executionHandler->interrupt();
        executionHandler->resume();
    }

    waitForConditionAndCheck("VMs did not resume after rapid cycles", [&]() {
        return VMManager::info().worldMode == VMManager::Mode::RunAll;
    });
    auto info = VMManager::info();
    CHECK(info.worldMode == VMManager::Mode::RunAll, "Should be running after cycles");

    TEST_LOG(failuresFound == initialFailures ? "PASS" : "FAIL");
}

static void testVMContextSwitching()
{
    TEST_LOG("\n=== VM Context Switching ===");

    int initialFailures = failuresFound;

    interruptAndWaitForStop("VMs did not stop for context switching");

    VM* firstTarget = executionHandler->vm();
    CHECK(firstTarget, "Should have target VM");

    Vector<VM*> allVMs;
    VMManager::forEachVM([&](VM& vm) {
        allVMs.append(&vm);
        return IterationStatus::Continue;
    });

    VLOG("Found ", allVMs.size(), " VMs");

    for (unsigned i = 0; i < allVMs.size() * CONTEXT_SWITCH_MULTIPLIER; ++i) {
        VM* targetVM = allVMs[i % allVMs.size()];
        uint64_t threadId = ExecutionHandler::mutatorThreadId(*targetVM);

        VLOG("Switching to VM ", RawPointer(targetVM), " thread ", threadId);
        executionHandler->switchTarget(threadId);

        CHECK(executionHandler->vm() == targetVM, "Switch failed");
    }

    executionHandler->resume();

    waitForConditionAndCheck("VMs did not resume after context switching", [&]() {
        return VMManager::info().worldMode == VMManager::Mode::RunAll;
    });

    TEST_LOG(failuresFound == initialFailures ? "PASS" : "FAIL");
}

static void testBreakpointContinueCycles()
{
    TEST_LOG("\n=== Breakpoint Continue Cycles ===");

    int initialFailures = failuresFound;

    interruptAndWaitForStop("VMs did not stop for setting breakpoints");

    unsigned breakpointsSet = setBreakpointsAtAllFunctionEntries(Breakpoint::Type::Regular);
    CHECK(breakpointsSet >= currentScript->expectedFunctions, "Should have set at least ", currentScript->expectedFunctions, " breakpoints");

    for (unsigned i = 0; i < BREAKPOINT_CONTINUE_CYCLES_COUNT; ++i) {
        VLOG("Continue cycle ", i);

        executionHandler->resume();

        waitForConditionAndCheck("VMs did not stop at breakpoint in continue cycle", [&]() {
            auto info = VMManager::info();
            bool stopped = info.worldMode == VMManager::Mode::Stopped && info.targetVM->debugState()->isStopped();
            return stopped;
        });

        DebugState* state = executionHandler->targetStateSafe();
        CHECK(state->atBreakpoint(), "Should stop at a breakpoint");
        VLOG("  Stopped at breakpoint in vm:", RawPointer(executionHandler->vm()));
    }

    clearBreakpointsAndResume("VMs did not resume after clearing breakpoints");

    TEST_LOG(failuresFound == initialFailures ? "PASS" : "FAIL");
}

static void testBreakpointSingleStepping()
{
    TEST_LOG("\n=== Breakpoint Single Stepping ===");

    int initialFailures = failuresFound;

    // 1. Interrupt to stop all VMs
    interruptAndWaitForStop("VMs did not stop for stepping test");

    // 2. Set breakpoints at ALL function entries
    unsigned breakpointsSet = setBreakpointsAtAllFunctionEntries(Breakpoint::Type::Regular);
    CHECK(breakpointsSet >= currentScript->expectedFunctions, "Should have set at least ", currentScript->expectedFunctions, " breakpoints");

    // 3. Continue - should hit a breakpoint immediately
    VLOG("Continuing execution (expecting breakpoint hit)...");
    executionHandler->resume();

    waitForConditionAndCheck("Did not hit breakpoint after resume", [&]() {
        auto info = VMManager::info();
        if (info.worldMode != VMManager::Mode::Stopped || !info.targetVM->debugState()->isStopped())
            return false;
        DebugState* state = executionHandler->targetStateSafe();
        return state && state->atBreakpoint();
    });

    DebugState* state = executionHandler->targetStateSafe();
    CHECK(state->atBreakpoint(), "Should be at breakpoint");

    // Record initial virtual address
    CHECK(state->stopData, "Should have stopData");
    JSC::Wasm::VirtualAddress currentAddress = state->stopData->address;
    VLOG("Hit breakpoint ", currentAddress);

    // 4. Single-step several times and verify we advance
    for (unsigned step = 0; step < SINGLE_STEPPING_CYCLES_COUNT; ++step) {
        VLOG("Step ", step + 1, "/", SINGLE_STEPPING_CYCLES_COUNT);

        // Simulate lldb behavior:
        // 1. If at Regular breakpoint: remove it, step, then re-insert it
        // 2. If at one-time breakpoint: just step directly
        Breakpoint* breakpoint = executionHandler->breakpointManager()->findBreakpoint(currentAddress);
        Breakpoint breakpointCopy;

        if (breakpoint) {
            breakpointCopy = *breakpoint;
            CHECK(breakpoint->type == Breakpoint::Type::Regular, "One-time breakpoints are cleared before stop. So, this must be a regular breakpoint");
            executionHandler->breakpointManager()->removeBreakpoint(currentAddress);
        }

        executionHandler->step();

        waitForConditionAndCheck("VMs did not stop at breakpoint in continue cycle", [&]() {
            auto info = VMManager::info();
            bool stopped = info.worldMode == VMManager::Mode::Stopped && info.targetVM->debugState()->isStopped();
            return stopped;
        });

        if (breakpoint)
            executionHandler->breakpointManager()->setBreakpoint(currentAddress, WTFMove(breakpointCopy));

        state = executionHandler->targetStateSafe();
        CHECK(state->atBreakpoint(), "Should be at breakpoint after step");

        JSC::Wasm::VirtualAddress afterStepAddress = state->stopData->address;
        VLOG("  After step: ", afterStepAddress);
        CHECK(afterStepAddress != currentAddress, "Virtual address should advance after step");

        currentAddress = afterStepAddress;
    }

    clearBreakpointsAndResume("VMs did not resume after stepping test");

    TEST_LOG(failuresFound == initialFailures ? "PASS" : "FAIL");
}

// ========== TEST ORCHESTRATION HELPERS ==========

// Waits for all VMs from previous test to be destroyed before starting next test.
static void waitForVMCleanupFromPreviousTest()
{
    TEST_LOG("Waiting for VMs from previous test to be destroyed...");
    bool vmsCleanedUp = waitForCondition([&]() {
        return !VMManager::info().numberOfVMs;
    });
    if (!vmsCleanedUp) {
        TEST_LOG("WARNING: VMs from previous test not cleaned up within timeout");
        TEST_LOG("Current VM count: ", VMManager::info().numberOfVMs);
    } else
        TEST_LOG("All VMs cleaned up successfully");
}

// Creates worker thread, runs test script, and waits for expected VMs to start.
// Returns true if VMs started successfully, false on timeout.
static bool setupScriptAndWaitForVMs(const TestScript& script, unsigned initialVMCount, RefPtr<Thread>& outWorkerThread)
{
    TEST_LOG("\nStarting worker thread with ", script.name, "...");

    outWorkerThread = Thread::create(WORKER_THREAD_NAME, [&script] {
        workerThreadTask(script.scriptGenerator());
    });

    TEST_LOG("Waiting for ", script.expectedVMs, " NEW VMs to start (total expected: ", initialVMCount + script.expectedVMs, ")...");
    bool vmsReady = waitForCondition([&]() {
        auto info = VMManager::info();
        unsigned currentCount = info.numberOfVMs;
        VLOG("Current VM count: ", currentCount, ", target: ", initialVMCount + script.expectedVMs);
        return currentCount >= initialVMCount + script.expectedVMs;
    });

    if (!vmsReady) {
        TEST_LOG("FAIL: VMs did not start within timeout for ", script.name);
        return false;
    }

    auto info = VMManager::info();
    TEST_LOG("Multi-VM setup complete: ", info.numberOfVMs, " VMs running");
    return true;
}

// Signals worker thread to exit, waits for completion, and resets ExecutionHandler state.
static void cleanupAfterScript(const TestScript& script, RefPtr<Thread>& workerThread)
{
    TEST_LOG("\nCleaning up ", script.name, "...");

    // Signal threads to exit their WASM loops
    TEST_LOG("Signaling thread to exit...");
    doneTesting = true;

    // Wait for thread to exit cleanly
    TEST_LOG("Waiting for thread to join...");
    workerThread->waitForCompletion();

    // Reset ExecutionHandler state for next test
    executionHandler->reset();

    // Reset flag for next test script
    doneTesting = false;
}

// ========== MAIN TEST RUNNER ==========

static int runTests()
{
    TEST_LOG("========================================");
    TEST_LOG("WASM Debugger Stress Tests");
    TEST_LOG("Testing ExecutionHandler with Real WASM");
    TEST_LOG("========================================");

    auto overallStartTime = MonotonicTime::now();
    int totalFailures = 0;

    // Get all registered test scripts
    auto scripts = TestScripts::getTestScripts();

    for (const auto& script : scripts) {
        TEST_LOG("\n");
        TEST_LOG("==========================================");
        TEST_LOG("Running tests with script: ", script.name);
        TEST_LOG(script.description);
        TEST_LOG("==========================================");

        waitForVMCleanupFromPreviousTest();

        auto scriptStartTime = MonotonicTime::now();
        failuresFound = 0; // Reset per-script failures
        currentScript = &script;

        // 1. SETUP with this script
        setupTestEnvironment(debugServer, executionHandler);

        // Track initial VM count (should be 0 after cleanup)
        unsigned initialVMCount = VMManager::info().numberOfVMs;
        TEST_LOG("Initial VM count: ", initialVMCount);

        RefPtr<Thread> workerThread;
        if (!setupScriptAndWaitForVMs(script, initialVMCount, workerThread)) {
            totalFailures++;
            continue; // Skip to next script
        }

        // 2. RUN ALL TESTS with this script
        testInterruptAndResume();
        testRapidInterruptResumeCycles();
        testVMContextSwitching();
        testBreakpointContinueCycles();
        testBreakpointSingleStepping();

        // 3. CLEANUP
        cleanupAfterScript(script, workerThread);

        auto scriptDuration = MonotonicTime::now() - scriptStartTime;

        // Report per-script results
        TEST_LOG("------------------------------------------");
        TEST_LOG("Script ", script.name, ": ",
            failuresFound ? "FAIL" : "PASS",
            " (", failuresFound, " failures, ",
            scriptDuration.millisecondsAs<long>(), " ms)");
        TEST_LOG("------------------------------------------");

        totalFailures += failuresFound;
    }

    auto overallDuration = MonotonicTime::now() - overallStartTime;

    TEST_LOG("\n========================================");
    TEST_LOG(totalFailures ? "FAIL" : "PASS", " - Overall Results");
    TEST_LOG("Total Time: ", overallDuration.millisecondsAs<long>(), " ms");
    TEST_LOG("Total Failures: ", totalFailures);
    TEST_LOG("========================================");

    return totalFailures > 0 ? 1 : 0;
}

#undef VLOG
#undef TEST_LOG
#undef CHECK

} // namespace ExecutionHandlerTest

int testExecutionHandler()
{
    return ExecutionHandlerTest::runTests();
}

#elif ENABLE(WEBASSEMBLY)

int testExecutionHandler()
{
    dataLogLn("WASM Debugger Stress Tests SKIPPED (REMOTE_INSPECTOR not enabled)");
    return 0;
}

#else

int testExecutionHandler()
{
    dataLogLn("WASM Debugger Stress Tests SKIPPED (WEBASSEMBLY not enabled)");
    return 0;
}

#endif // ENABLE(WEBASSEMBLY)
