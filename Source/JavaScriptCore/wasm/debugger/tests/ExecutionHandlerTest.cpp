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

#include <wtf/DataLog.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

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
UNUSED_FUNCTION bool doneTesting = false;

#define VLOG(...) dataLogLnIf(verboseLogging, __VA_ARGS__)
#define TEST_LOG(...) dataLogLn(__VA_ARGS__)

#define CHECK(condition, ...)                                   \
    do {                                                        \
        if (!(condition)) {                                     \
            dataLogLn("FAIL: ", #condition, ": ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__);        \
            CRASH();                                            \
        }                                                       \
    } while (false)

static void waitForConditionAndCheck(const char* errorMessage, std::function<bool()> predicate)
{
    bool result = waitForCondition(predicate);
    CHECK(result, errorMessage);
}

// ========== HELPER FUNCTIONS ==========

static void validateStop()
{
    auto info = VMManager::info();
    CHECK(info.worldMode == VMManager::Mode::Stopped, "All VMs should be stopped");
    auto* state = executionHandler->debuggeeStateSafe();
    CHECK(state->isStopped(), "Debuggee VM should be stopped");
    CHECK(info.targetVM == executionHandler->debuggeeVM(), "VMManager's targetVM should match ExecutionHandler's debuggee VM");
}

static void interrupt()
{
    executionHandler->interrupt();
    validateStop();
}

static void resume()
{
    executionHandler->resume();

    auto info = VMManager::info();
    CHECK(info.worldMode == VMManager::Mode::RunAll, "All VMs should be running");
    auto* state = executionHandler->debuggeeStateSafe();
    CHECK(state->isRunning(), "Debuggee VM should be running");
}

static void switchTarget(VM* newDebuggee)
{
    uint64_t threadId = ExecutionHandler::threadId(*newDebuggee);
    executionHandler->switchTarget(threadId);
    validateStop();
    CHECK(executionHandler->debuggeeVM() == newDebuggee, "Switch to new debuggee failed");
}

static void setBreakpointsAtAllFunctionEntries(Breakpoint::Type type)
{
    VLOG("Setting breakpoints at all function entries...");
    unsigned count = 0;

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
            count++;
        }
    }

    VLOG("Set ", count, " breakpoints total");
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

static void testRapidInterruptResumeCycles()
{
    TEST_LOG("\n=== Rapid Interrupt/Resume Cycles ===");
    for (unsigned i = 0; i < RAPID_CYCLES_COUNT; ++i) {
        VLOG("Cycle ", i);
        interrupt();
        resume();
    }

    TEST_LOG("PASS");
}

static void testVMContextSwitching()
{
    TEST_LOG("\n=== VM Context Switching ===");

    interrupt();

    Vector<VM*> allVMs;
    VMManager::forEachVM([&](VM& vm) {
        allVMs.append(&vm);
        return IterationStatus::Continue;
    });
    VLOG("Found ", allVMs.size(), " VMs");

    for (unsigned i = 0; i < allVMs.size() * CONTEXT_SWITCH_MULTIPLIER; ++i) {
        VM* nextDebuggee = allVMs[i % allVMs.size()];
        switchTarget(nextDebuggee);
    }

    resume();

    TEST_LOG("PASS");
}

static void testBreakpointContinueCycles()
{
    TEST_LOG("\n=== Breakpoint Continue Cycles ===");

    interrupt();

    setBreakpointsAtAllFunctionEntries(Breakpoint::Type::Regular);
    for (unsigned i = 0; i < BREAKPOINT_CONTINUE_CYCLES_COUNT; ++i) {
        VLOG("Continue cycle ", i);

        executionHandler->resume();

        waitForConditionAndCheck("VMs did not stop at breakpoint in continue cycle", [&]() {
            auto info = VMManager::info();
            bool stopped = info.worldMode == VMManager::Mode::Stopped && info.targetVM->debugState()->isStopped();
            return stopped;
        });

        DebugState* state = executionHandler->debuggeeStateSafe();
        CHECK(state->atBreakpoint(), "Should stop at a breakpoint");
        VLOG("  Stopped at breakpoint in vm:", RawPointer(executionHandler->debuggeeVM()));
    }

    clearBreakpointsAndResume("VMs did not resume after clearing breakpoints");

    TEST_LOG("PASS");
}

static void testBreakpointSingleStepping()
{
    TEST_LOG("\n=== Breakpoint Single Stepping ===");

    int initialFailures = failuresFound;

    // 1. Interrupt to stop all VMs
    interrupt();

    // 2. Set breakpoints at ALL function entries
    setBreakpointsAtAllFunctionEntries(Breakpoint::Type::Regular);

    // 3. Continue - should hit a breakpoint immediately
    VLOG("Continuing execution (expecting breakpoint hit)...");
    executionHandler->resume();

    waitForConditionAndCheck("Did not hit breakpoint after resume", [&]() {
        auto info = VMManager::info();
        if (info.worldMode != VMManager::Mode::Stopped || !info.targetVM->debugState()->isStopped())
            return false;
        DebugState* state = executionHandler->debuggeeStateSafe();
        return state && state->atBreakpoint();
    });

    DebugState* state = executionHandler->debuggeeStateSafe();
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

        waitForConditionAndCheck("VMs did not stop after step", [&]() {
            auto info = VMManager::info();
            bool stopped = info.worldMode == VMManager::Mode::Stopped && info.targetVM->debugState()->isStopped();
            return stopped;
        });

        if (breakpoint)
            executionHandler->breakpointManager()->setBreakpoint(currentAddress, WTF::move(breakpointCopy));

        state = executionHandler->debuggeeStateSafe();
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

static void waitForVMCleanupFromPreviousTest()
{
    TEST_LOG("Waiting for VMs from previous test to be destroyed...");
    bool cleanedUp = waitForCondition([]() {
        return !VMManager::info().numberOfVMs;
    });

    if (!cleanedUp)
        TEST_LOG("WARNING: VMs not cleaned up within timeout (count: ", VMManager::info().numberOfVMs, ")");
    else
        TEST_LOG("All VMs cleaned up successfully");
}

// FIXME: Add tests for VM lifecycle edge cases (construction, initialization, instance registration) and
// interrupt() race conditions. Currently we only wait for VM construction and instance registration, which
// doesn't guarantee VMs are actively running code that checks traps.
static bool setupScriptAndWaitForVMs(const TestScript& script, unsigned initialVMCount, RefPtr<Thread>& outWorkerThread)
{
    CHECK(!initialVMCount, "Expected initial VM count to be 0, got ", initialVMCount);

    ModuleManager& moduleManager = debugServer->moduleManager();
    unsigned initialInstanceId = moduleManager.nextInstanceId();

    TEST_LOG("\nStarting worker thread with ", script.name, "...");
    outWorkerThread = Thread::create(WORKER_THREAD_NAME, [&script] {
        workerThreadTask(script.scriptGenerator());
    });

    TEST_LOG("Waiting for ", script.expectedVMs, " VMs to start...");
    if (!waitForCondition([&]() { return VMManager::info().numberOfVMs >= script.expectedVMs; })) {
        TEST_LOG("FAIL: VMs did not start within timeout");
        return false;
    }

    unsigned expectedInstanceId = initialInstanceId + script.expectedVMs;
    TEST_LOG("Waiting for ", script.expectedVMs, " instances (ID: ", initialInstanceId, " -> ", expectedInstanceId, ")...");
    if (!waitForCondition([&]() { return moduleManager.nextInstanceId() >= expectedInstanceId; })) {
        TEST_LOG("FAIL: Instances timeout (expected: ", expectedInstanceId, ", got: ", moduleManager.nextInstanceId(), ")");
        return false;
    }

    unsigned finalInstanceId = moduleManager.nextInstanceId();
    unsigned instanceIncrement = finalInstanceId - initialInstanceId;
    CHECK(instanceIncrement == script.expectedVMs, "Expected ", script.expectedVMs, " new instances, got ", instanceIncrement);

    TEST_LOG("Setup complete: ", VMManager::info().numberOfVMs, " VMs, ", instanceIncrement, " instances (ID: ", initialInstanceId, " -> ", finalInstanceId, ")");
    return true;
}

static void cleanupAfterScript(const TestScript& script, RefPtr<Thread>& workerThread)
{
    TEST_LOG("\nCleaning up ", script.name, "...");
    doneTesting = true;
    workerThread->waitForCompletion();
    executionHandler->reset();
    doneTesting = false;
}

// ========== MAIN TEST RUNNER ==========

UNUSED_FUNCTION static int runTests()
{
    TEST_LOG("========================================");
    TEST_LOG("WASM Debugger Stress Tests");
    TEST_LOG("Testing ExecutionHandler with Real WASM");
    TEST_LOG("========================================");

    auto overallStartTime = MonotonicTime::now();
    int totalFailures = 0;

    setupTestEnvironment(debugServer, executionHandler);
    auto scripts = TestScripts::getTestScripts();

    for (const auto& script : scripts) {
        TEST_LOG("\n==========================================");
        TEST_LOG("Running tests with script: ", script.name);
        TEST_LOG(script.description);
        TEST_LOG("==========================================");

        auto scriptStartTime = MonotonicTime::now();
        failuresFound = 0;
        currentScript = &script;

        waitForVMCleanupFromPreviousTest();

        RefPtr<Thread> workerThread;
        if (!setupScriptAndWaitForVMs(script, VMManager::info().numberOfVMs, workerThread)) {
            totalFailures++;
            continue;
        }

        testRapidInterruptResumeCycles();
        testVMContextSwitching();
        testBreakpointContinueCycles();
        testBreakpointSingleStepping();

        cleanupAfterScript(script, workerThread);

        auto scriptDuration = MonotonicTime::now() - scriptStartTime;
        TEST_LOG("------------------------------------------");
        TEST_LOG("Script ", script.name, ": ", failuresFound ? "FAIL" : "PASS",
            " (", failuresFound, " failures, ", scriptDuration.millisecondsAs<long>(), " ms)");
        TEST_LOG("------------------------------------------");

        totalFailures += failuresFound;
    }

    auto overallDuration = MonotonicTime::now() - overallStartTime;

    TEST_LOG("\n========================================");
    TEST_LOG(totalFailures ? "FAIL" : "PASS", " - Overall Results");
    TEST_LOG("Total Time: ", overallDuration.millisecondsAs<long>(), " ms");
    TEST_LOG("Total Failures: ", totalFailures);
    TEST_LOG("========================================");

    return totalFailures;
}

#undef VLOG
#undef TEST_LOG
#undef CHECK

} // namespace ExecutionHandlerTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)

int testExecutionHandler()
{
#if ENABLE(WEBASSEMBLY_DEBUGGER) && CPU(ARM64)
    return ExecutionHandlerTest::runTests();
#else
    dataLogLn("WASM Debugger Stress Tests SKIPPED (only supported on ARM64)");
    return 0;
#endif
}
