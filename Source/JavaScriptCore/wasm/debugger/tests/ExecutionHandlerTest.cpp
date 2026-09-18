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
#include "WasmCallee.h"
#include "WasmCalleeGroup.h"
#include "WasmDebugServer.h"
#include "WasmDebugServerUtilities.h"
#include "WasmExecutionHandler.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmModuleManager.h"
#include "WasmVirtualAddress.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace ExecutionHandlerTest {

using ExecutionHandlerTestSupport::getReplyCount;
using ExecutionHandlerTestSupport::setupTestEnvironment;
using ExecutionHandlerTestSupport::verboseLogging;
using ExecutionHandlerTestSupport::waitForCondition;
using ExecutionHandlerTestSupport::workerThreadTask;
using JSC::JSWebAssemblyInstance;
using JSC::VM;
using JSC::VMManager;
using JSC::Wasm::Breakpoint;
using JSC::Wasm::DebugServer;
using JSC::Wasm::DebugState;
using JSC::Wasm::ExecutionHandler;
using JSC::Wasm::FunctionSpaceIndex;
using JSC::Wasm::ModuleInformation;
using JSC::Wasm::ModuleManager;
using TestScripts::TestScript;

// ========== Test runtime state ==========

static constexpr unsigned STRESS_TEST_ITERATIONS = 1000;
static constexpr ASCIILiteral WORKER_THREAD_NAME = "WasmStressTest"_s;

static uint32_t failuresFound = 0;
static uint32_t expectedVMCount = 0;
static DebugServer* debugServer = nullptr;
static ExecutionHandler* executionHandler = nullptr;
UNUSED_FUNCTION static const TestScript* currentScript = nullptr;
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

static void waitForConditionAndCheck(ASCIILiteral errorMessage, std::function<bool()> predicate)
{
    bool result = waitForCondition(predicate);
    CHECK(result, errorMessage);
}

// ========== HELPER FUNCTIONS ==========

static void validateStop()
{
    auto info = VMManager::info();
    CHECK(info.worldMode == VMManager::Mode::Stopped, "All VMs should be stopped");
    CHECK(info.targetVM == executionHandler->debuggeeVM(), "VMManager's targetVM should match ExecutionHandler's debuggee VM");
    uint32_t stoppedCount = 0;
    VMManager::forEachVM([&](VM& vm) {
        CHECK(vm.debugState()->isStopped, "VM should be stopped");
        stoppedCount++;
        return IterationStatus::Continue;
    });
    CHECK(stoppedCount == expectedVMCount, "Expected ", expectedVMCount, " stopped VMs, got ", stoppedCount);
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
    uint32_t runningCount = 0;
    VMManager::forEachVM([&](VM& vm) {
        CHECK(!vm.debugState()->isStopped, "VM should be running");
        runningCount++;
        return IterationStatus::Continue;
    });
    CHECK(runningCount == expectedVMCount, "Expected ", expectedVMCount, " running VMs, got ", runningCount);
}

static void switchTarget(VM* newDebuggee)
{
    executionHandler->switchTarget(newDebuggee->identifier().toUInt64());
    validateStop();
    CHECK(executionHandler->debuggeeVM() == newDebuggee, "Switch to new debuggee failed");
}

// The address LLDB would install a site at for this instance's view of the callee's entry.
static JSC::Wasm::VirtualAddress entryAddress(JSWebAssemblyInstance* instance, JSC::Wasm::IPIntCallee* callee)
{
    return JSC::Wasm::VirtualAddress::toVirtual(instance, callee->functionIndex(), callee->bytecode());
}

// Installs a site at every function entry of every live instance, the way LLDB does once a symbol
// resolves in each instance's library.
static void setBreakpointsAtAllFunctionEntries()
{
    VLOG("Setting breakpoints at all function entries...");
    unsigned count = 0;

    ModuleManager& moduleManager = debugServer->moduleManager();
    uint32_t maxInstanceId = moduleManager.nextInstanceId();
    auto* breakpointManager = executionHandler->breakpointManager();

    for (uint32_t instanceId = 0; instanceId < maxInstanceId; ++instanceId) {
        JSWebAssemblyInstance* instance = moduleManager.jsInstance(instanceId);
        if (!instance)
            continue;

        auto& moduleInfo = instance->module().moduleInformation();
        uint32_t internalCount = moduleInfo.internalFunctionCount();

        VLOG("  Instance ", instanceId, ": ", internalCount, " functions");

        for (uint32_t funcIndex = 0; funcIndex < internalCount; ++funcIndex) {
            FunctionSpaceIndex spaceIndex = moduleInfo.toSpaceIndex(JSC::Wasm::FunctionCodeIndex(funcIndex));
            auto callee = instance->calleeGroup()->ipintCalleeFromFunctionIndexSpace(spaceIndex);
            // Instances of one module share the bytecode a site patches, but each instance needs a
            // site of its own: a site only stops the instance its address names.
            breakpointManager->setBreakpointAt(entryAddress(instance, callee.ptr()), moduleInfo, const_cast<uint8_t*>(callee->bytecode()));
            count++;
        }
    }

    VLOG("Set ", count, " breakpoints total");
}

static void clearBreakpointsAndResume()
{
    executionHandler->breakpointManager()->clearAllBreakpoints();
    resume();
}

// ========== BASIC TESTS ==========

static void testRapidInterruptResumeCycles()
{
    TEST_LOG("\n=== Rapid Interrupt/Resume Cycles ===");
    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
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

    for (unsigned i = 0; i < allVMs.size() * STRESS_TEST_ITERATIONS; ++i) {
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

    setBreakpointsAtAllFunctionEntries();
    for (unsigned i = 0; i < STRESS_TEST_ITERATIONS; ++i) {
        VLOG("Continue cycle ", i);

        unsigned expectedReplyCount = getReplyCount() + 1;
        executionHandler->resume();

        waitForConditionAndCheck("VMs did not stop at breakpoint in continue cycle"_s, [&]() {
            return getReplyCount() == expectedReplyCount;
        });

        DebugState* state = executionHandler->debuggeeStateForTest();
        CHECK(state->isStoppedAtBytecode(), "Should stop at a breakpoint");
        VLOG("  Stopped at breakpoint in vm:", RawPointer(executionHandler->debuggeeVM()));
    }

    clearBreakpointsAndResume();

    TEST_LOG("PASS");
}

static void testBreakpointSingleStepping()
{
    TEST_LOG("\n=== Breakpoint Single Stepping ===");

    uint32_t initialFailures = failuresFound;

    // 1. Interrupt to stop all VMs
    interrupt();

    // 2. Set breakpoints at ALL function entries
    setBreakpointsAtAllFunctionEntries();

    // 3. Continue - should hit a breakpoint immediately
    VLOG("Continuing execution (expecting breakpoint hit)...");
    unsigned expectedReplyCount = getReplyCount() + 1;
    executionHandler->resume();

    waitForConditionAndCheck("Did not hit breakpoint after resume"_s, [&]() {
        bool stopped = getReplyCount() == expectedReplyCount;
        if (!stopped)
            return false;
        return executionHandler->debuggeeStateForTest()->isStoppedAtBytecode();
    });

    DebugState* state = executionHandler->debuggeeStateForTest();
    CHECK(state->isStoppedAtBytecode(), "Should be at breakpoint");

    // Record initial virtual address
    CHECK(state->stopData, "Should have stopData");
    JSC::Wasm::VirtualAddress beforeStepAddress = state->stopData->address;
    VLOG("Hit breakpoint ", beforeStepAddress);

    // 4. Single-step several times and verify we advance
    for (unsigned step = 0; step < STRESS_TEST_ITERATIONS; ++step) {
        VLOG("Step ", step + 1, "/", STRESS_TEST_ITERATIONS);

        // Simulate lldb behavior:
        // 1. If stopped at a site: remove it, step, then re-arm it
        // 2. If stopped at a one-time breakpoint: just step directly
        uint8_t* stoppedPC = state->stopData->pc;
        JSC::Wasm::VirtualAddress stoppedAddress = state->stopData->address;
        // Capture the owner before stepping; the step clears stopData.
        RefPtr<const JSC::Wasm::ModuleInformation> owner = &state->stopData->instance->moduleInformation();
        bool hadSite = executionHandler->breakpointManager()->removeBreakpointAt(stoppedAddress);
        CHECK(hadSite == (state->stopReason == DebugState::Reason::Breakpoint), "A breakpoint stop must be reported at an address the stopped instance holds a site at");

        unsigned expectedReplyCount = getReplyCount() + 1;
        executionHandler->step();

        waitForConditionAndCheck("VMs did not stop after step"_s, [&]() {
            return getReplyCount() == expectedReplyCount;
        });

        if (hadSite)
            executionHandler->breakpointManager()->setBreakpointAt(stoppedAddress, *owner, stoppedPC);

        state = executionHandler->debuggeeStateForTest();
        CHECK(state->isStoppedAtBytecode(), "Should be at breakpoint after step");

        JSC::Wasm::VirtualAddress afterStepAddress = state->stopData->address;
        VLOG("  After step: ", afterStepAddress);
        CHECK(afterStepAddress != beforeStepAddress, "Virtual address should advance after step");

        beforeStepAddress = afterStepAddress;
    }

    clearBreakpointsAndResume();

    TEST_LOG(failuresFound == initialFailures ? "PASS" : "FAIL");
}

// Collects the live instances of each module, keyed by the module they were instantiated from.
using ModuleToInstanceIds = UncheckedKeyHashMap<JSC::Wasm::Module*, Vector<uint32_t>>;
static ModuleToInstanceIds liveInstancesByModule()
{
    ModuleManager& moduleManager = debugServer->moduleManager();
    uint32_t maxInstanceId = moduleManager.nextInstanceId();

    ModuleToInstanceIds result;
    for (uint32_t instanceId = 0; instanceId < maxInstanceId; ++instanceId) {
        JSWebAssemblyInstance* instance = moduleManager.jsInstance(instanceId);
        if (!instance)
            continue;
        result.add(&instance->module(), Vector<uint32_t>()).iterator->value.append(instanceId);
    }
    return result;
}

static void testInstanceScopedAddressing()
{
    TEST_LOG("\n=== Instance Scoped Addressing ===");

    interrupt();

    ModuleManager& moduleManager = debugServer->moduleManager();
    auto instancesByModule = liveInstancesByModule();
    CHECK(!instancesByModule.isEmpty(), "Expected at least one live instance while stopped");

    unsigned liveInstances = 0;
    unsigned sharedModules = 0;
    UncheckedKeyHashSet<uint64_t> codeBaseAddresses;
    for (const auto& pair : instancesByModule) {
        if (pair.value.size() > 1)
            sharedModules++;
        for (uint32_t instanceId : pair.value) {
            liveInstances++;
            JSWebAssemblyInstance* instance = moduleManager.jsInstance(instanceId);
            CHECK(instance, "Instance ", instanceId, " should still resolve");
            CHECK(instance->debugId() == instanceId, "Instance ", instanceId, " should carry its own debug id");

            // Every instance owns a slice of the address space, even siblings sharing a module.
            uint64_t codeBase = JSC::Wasm::VirtualAddress::createModule(instanceId).value();
            CHECK(codeBaseAddresses.add(codeBase).isNewEntry, "Instance ", instanceId, " should get an address range of its own");
        }
    }

    CHECK(!moduleManager.jsInstance(moduleManager.nextInstanceId()), "An unregistered instance ID should not resolve");

    resume();

    TEST_LOG("PASS (", liveInstances, " live instance(s), ", sharedModules, " module(s) with several)");
}

static void testSharedBytecodeBreakpoints()
{
    TEST_LOG("\n=== Shared Bytecode Breakpoints ===");

    interrupt();

    ModuleManager& moduleManager = debugServer->moduleManager();
    auto* breakpointManager = executionHandler->breakpointManager();

    // Siblings of one module execute the same bytecode buffer, so a breakpoint set through any of
    // them patches one and the same byte. LLDB still installs a site per instance, and a site
    // speaks only for the instance its address names.
    unsigned modulesChecked = 0;
    for (const auto& pair : liveInstancesByModule()) {
        if (pair.value.size() < 2)
            continue;
        modulesChecked++;

        const uint8_t* sharedPC = nullptr;
        const ModuleInformation* owner = nullptr;
        uint8_t originalBytecode = 0;
        Vector<JSC::Wasm::VirtualAddress> addresses;
        for (uint32_t instanceId : pair.value) {
            JSWebAssemblyInstance* instance = moduleManager.jsInstance(instanceId);
            auto& moduleInfo = instance->module().moduleInformation();
            FunctionSpaceIndex spaceIndex = moduleInfo.toSpaceIndex(JSC::Wasm::FunctionCodeIndex(0));
            auto callee = instance->calleeGroup()->ipintCalleeFromFunctionIndexSpace(spaceIndex);

            if (!sharedPC) {
                sharedPC = callee->bytecode();
                owner = &moduleInfo;
                originalBytecode = *sharedPC;
            } else
                CHECK(sharedPC == callee->bytecode(), "Instances of module ", RawPointer(pair.key), " should share function 0's bytecode");

            // Instance-scoped addresses, one shared patch.
            auto address = entryAddress(instance, callee.ptr());
            CHECK(address.instanceId() == instanceId, "A pc in instance ", instanceId, " should carry its instance id");
            addresses.append(address);
        }
        uint8_t* pc = const_cast<uint8_t*>(sharedPC);

        // One site patches the shared byte, but only the instance it names stops there.
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        CHECK(*sharedPC != originalBytecode, "A site should patch the shared bytecode");
        auto breakpoint = breakpointManager->breakpointAt(sharedPC);
        CHECK(breakpoint, "The site should reach a breakpoint at ", addresses[0]);
        CHECK(breakpoint->originalBytecode == originalBytecode, "The patch must record the opcode it displaced");
        CHECK(breakpoint->owner == owner, "The breakpoint should pin the module that owns its bytecode");

        auto action = breakpointManager->trapActionFor(sharedPC, addresses[0]);
        CHECK(action && action->stopReason == DebugState::Reason::Breakpoint, "The instance holding the site should stop");
        CHECK(action->displacedOpcode == originalBytecode, "Resuming should dispatch the displaced opcode");
        for (size_t i = 1; i < addresses.size(); ++i) {
            action = breakpointManager->trapActionFor(sharedPC, addresses[i]);
            CHECK(action && !action->stopReason, "Instance ", addresses[i].instanceId(), " holds no site here and must not stop");
            CHECK(action->displacedOpcode == originalBytecode, "It should still resume through the patch");
        }

        // Re-arming a site LLDB already holds must not count twice.
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        CHECK(breakpointManager->removeBreakpointAt(addresses[0]), "The site should be removable");
        CHECK(*sharedPC == originalBytecode, "Removing the last site should restore the displaced opcode");
        CHECK(!breakpointManager->removeBreakpointAt(addresses[0]), "Removing it again should report there was nothing to remove");
        CHECK(!breakpointManager->breakpointAt(sharedPC), "A patch with no reason to live should leave no breakpoint");

        // LLDB addresses that shared byte once per instance, so removing one of its sites must
        // leave the byte patched for the others.
        for (auto address : addresses)
            breakpointManager->setBreakpointAt(address, *owner, pc);
        CHECK(*sharedPC != originalBytecode, "Installing through every instance should patch the shared bytecode once");

        for (size_t i = 0; i < addresses.size() - 1; ++i) {
            CHECK(breakpointManager->removeBreakpointAt(addresses[i]), "Site ", addresses[i], " should be removable");
            CHECK(*sharedPC != originalBytecode, "The bytecode must stay patched while another site refers to it");
            action = breakpointManager->trapActionFor(sharedPC, addresses[i]);
            CHECK(action && !action->stopReason, "The instance whose site was removed must stop no longer");
        }
        CHECK(breakpointManager->removeBreakpointAt(addresses.last()), "The last site should be removable");
        CHECK(*sharedPC == originalBytecode, "Removing the last site should restore the displaced opcode");

        // To resume from a breakpoint LLDB removes the site at the current PC, steps over it, and
        // reinstalls it. With a sibling instance still holding a site the patch stays, so the step
        // lands on a PC that already has a breakpoint.
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        breakpointManager->setBreakpointAt(addresses[1], *owner, pc);
        CHECK(breakpointManager->removeBreakpointAt(addresses[0]), "Stepping over a site should remove it");
        breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(*owner, pc);
        CHECK(breakpointManager->hasOneTimeBreakpoints(), "A step onto a PC held by a site must still register as one-time");
        action = breakpointManager->trapActionFor(sharedPC, addresses[1]);
        CHECK(action && action->stopReason == DebugState::Reason::Breakpoint, "A step must not mask the site's stop reason");
        // A step belongs to the debuggee VM rather than to an instance, so whichever instance
        // reaches the PC completes it.
        action = breakpointManager->trapActionFor(sharedPC, addresses[0]);
        CHECK(action && action->stopReason == DebugState::Reason::Step, "An instance holding no site should still complete the step");

        // The step outlives the last site: LLDB removes it before the step lands.
        CHECK(breakpointManager->removeBreakpointAt(addresses[1]), "The remaining site should be removable");
        CHECK(*sharedPC != originalBytecode, "The bytecode must stay patched for the step in progress");
        breakpointManager->clearAllOneTimeBreakpoints();
        CHECK(*sharedPC == originalBytecode, "Completing the step should restore the displaced opcode");

        // A site outliving the step keeps the patch.
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(*owner, pc);
        breakpointManager->clearAllOneTimeBreakpoints();
        CHECK(*sharedPC != originalBytecode, "Completing the step must not disarm the site sharing its PC");
        CHECK(breakpointManager->removeBreakpointAt(addresses[0]), "The site should outlive the step");
        CHECK(*sharedPC == originalBytecode, "Removing the last site should restore the displaced opcode");

        // The reverse order. Unreachable today since every stop clears claims before LLDB is
        // notified, but stopTheWorld's FIXME would preserve them, so the order must not matter.
        breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(*owner, pc);
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        action = breakpointManager->trapActionFor(sharedPC, addresses[0]);
        CHECK(action && action->stopReason == DebugState::Reason::Breakpoint, "A site installed after a claim still outranks it");
        action = breakpointManager->trapActionFor(sharedPC, addresses[1]);
        CHECK(action && action->stopReason == DebugState::Reason::Step, "A siteless instance still completes a claim armed before the site");
        breakpointManager->clearAllOneTimeBreakpoints();
        CHECK(*sharedPC != originalBytecode, "Clearing the claim must not disarm the site installed after it");
        CHECK(breakpointManager->removeBreakpointAt(addresses[0]), "The site outliving that claim should be removable");
        CHECK(*sharedPC == originalBytecode, "Removing it should restore the displaced opcode");

        // And a claim outliving a site installed after it.
        breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(*owner, pc);
        breakpointManager->setBreakpointAt(addresses[0], *owner, pc);
        CHECK(breakpointManager->removeBreakpointAt(addresses[0]), "That site should be removable");
        CHECK(*sharedPC != originalBytecode, "The claim armed before it must keep the byte patched");
        breakpointManager->clearAllOneTimeBreakpoints();
        CHECK(*sharedPC == originalBytecode, "Clearing the last claim should restore the displaced opcode");

        // Nothing patched the byte, so a trap there is a genuine unreachable.
        CHECK(!breakpointManager->trapActionFor(sharedPC, addresses[0]), "An unpatched PC should report no breakpoint");

        // A collected instance's library is unloaded without a z0 per site, so its sites are
        // dropped by instance id instead. The patch must survive until the siblings let go.
        for (auto address : addresses)
            breakpointManager->setBreakpointAt(address, *owner, pc);
        for (size_t i = 0; i < addresses.size() - 1; ++i) {
            breakpointManager->removeSitesForInstance(addresses[i].instanceId());
            CHECK(*sharedPC != originalBytecode, "The bytecode must stay patched while a sibling holds a site");
            CHECK(!breakpointManager->removeBreakpointAt(addresses[i]), "A dropped site must not be removable twice");
        }
        breakpointManager->removeSitesForInstance(addresses.last().instanceId());
        CHECK(*sharedPC == originalBytecode, "Dropping the last instance should restore the displaced opcode");
        CHECK(!breakpointManager->breakpointAt(sharedPC), "Dropping every site should leave no breakpoint");
    }

    resume();

    TEST_LOG(modulesChecked ? "PASS" : "PASS (no module with several live instances)");
}

static void testPatchLifetime()
{
    TEST_LOG("\n=== Breakpoint Patch Lifetime ===");

    interrupt();

    ModuleManager& moduleManager = debugServer->moduleManager();
    auto* breakpointManager = executionHandler->breakpointManager();

    uint32_t maxInstanceId = moduleManager.nextInstanceId();
    JSWebAssemblyInstance* instance = nullptr;
    for (uint32_t instanceId = 0; instanceId < maxInstanceId && !instance; ++instanceId)
        instance = moduleManager.jsInstance(instanceId);
    CHECK(instance, "Expected at least one live instance while stopped");

    auto& moduleInfo = instance->module().moduleInformation();
    FunctionSpaceIndex spaceIndex = moduleInfo.toSpaceIndex(JSC::Wasm::FunctionCodeIndex(0));
    auto callee = instance->calleeGroup()->ipintCalleeFromFunctionIndexSpace(spaceIndex);
    auto address = entryAddress(instance, callee.ptr());
    uint8_t* pc = const_cast<uint8_t*>(callee->bytecode());
    const uint8_t originalBytecode = *pc;

    CHECK(!breakpointManager->removeBreakpointAt(address), "Removing an address that holds no site should report there was nothing to remove");

    breakpointManager->setBreakpointAt(address, moduleInfo, pc);
    CHECK(*pc != originalBytecode, "Setting a breakpoint should patch the bytecode");

    // Re-arming an existing site must not displace the patch byte over the real opcode.
    breakpointManager->setBreakpointAt(address, moduleInfo, pc);

    // A step target on a byte a site already patched: clearing it must leave the site armed.
    breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(moduleInfo, pc);
    CHECK(breakpointManager->hasOneTimeBreakpoints(), "The step target should be pending");
    breakpointManager->clearAllOneTimeBreakpoints();
    CHECK(!breakpointManager->hasOneTimeBreakpoints(), "Clearing should drop the step target");
    CHECK(*pc != originalBytecode, "The bytecode must stay patched while the site refers to it");

    CHECK(breakpointManager->removeBreakpointAt(address), "The site should be removable");
    CHECK(*pc == originalBytecode, "Removing the last reference should restore the displaced opcode");

    // The other order: the site goes away first, the step target holds the patch.
    breakpointManager->setBreakpointAt(address, moduleInfo, pc);
    breakpointManager->setOneTimeBreakpoint<DebugState::Reason::Step>(moduleInfo, pc);
    CHECK(breakpointManager->removeBreakpointAt(address), "The site should be removable");
    CHECK(*pc != originalBytecode, "The bytecode must stay patched while the step target refers to it");
    breakpointManager->clearAllOneTimeBreakpoints();
    CHECK(*pc == originalBytecode, "Clearing the last reference should restore the displaced opcode");

    resume();

    TEST_LOG("PASS");
}

// ========== TEST ORCHESTRATION HELPERS ==========
static void waitForVMCleanupFromPreviousTest()
{
    TEST_LOG("Waiting for VMs from previous test to be destroyed...");
    bool cleanedUp = waitForCondition([]() {
        return !VMManager::info().numberOfVMs;
    });

    CHECK(cleanedUp, "VMs not cleaned up within timeout (count: ", VMManager::info().numberOfVMs, ")");
    TEST_LOG("All VMs cleaned up successfully");
}

// setupScriptAndWaitForVMs ensures all VMs are constructed, instances registered, entered with owner threads,
// and actively running before tests start.
static bool setupScriptAndWaitForVMs(const TestScript& script, RefPtr<Thread>& outWorkerThread)
{
    ModuleManager& moduleManager = debugServer->moduleManager();
    unsigned initialInstanceId = moduleManager.nextInstanceId();
    unsigned expectedInstanceId = initialInstanceId + script.expectedInstances;

    TEST_LOG("\nStarting worker thread with ", script.name, "...");
    outWorkerThread = Thread::create(WORKER_THREAD_NAME, [&script] {
        workerThreadTask(script.scriptGenerator());
    });

    TEST_LOG("Waiting for ", script.expectedVMs, " VMs to be fully initialized and running...");
    if (!waitForCondition([&]() {
        auto info = VMManager::info();

        if (info.numberOfVMs != script.expectedVMs)
            return false;
        if (moduleManager.nextInstanceId() != expectedInstanceId)
            return false;
        if (info.worldMode != VMManager::Mode::RunAll)
            return false;
        if (info.numberOfStoppedVMs)
            return false;

        unsigned activeCount = 0;
        VMManager::forEachVM([&](VM& vm) {
            if (vm.isEntered())
                activeCount++;
            return IterationStatus::Continue;
        });
        return activeCount == script.expectedVMs;
    })) {
        auto info = VMManager::info();
        TEST_LOG("FAIL: VMs not ready within timeout");
        TEST_LOG("  numberOfVMs=", info.numberOfVMs, " (expected ", script.expectedVMs, ")");
        TEST_LOG("  instances=", moduleManager.nextInstanceId(), " (expected ", expectedInstanceId, ")");
        TEST_LOG("  worldMode=", (int)info.worldMode, " (expected RunAll=", (int)VMManager::Mode::RunAll, ")");
        TEST_LOG("  numberOfStoppedVMs=", info.numberOfStoppedVMs, " (expected 0)");
        return false;
    }

    TEST_LOG("Setup complete: ", script.expectedVMs, " VMs constructed, instances registered, all entered and running");
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
        expectedVMCount = script.expectedVMs;
        currentScript = &script;

        waitForVMCleanupFromPreviousTest();

        RefPtr<Thread> workerThread;
        RELEASE_ASSERT(setupScriptAndWaitForVMs(script, workerThread));

        testRapidInterruptResumeCycles();
        testVMContextSwitching();
        testBreakpointContinueCycles();
        testBreakpointSingleStepping();
        testInstanceScopedAddressing();
        testPatchLifetime();
        testSharedBytecodeBreakpoints();

        cleanupAfterScript(script, workerThread);

        auto scriptDuration = MonotonicTime::now() - scriptStartTime;
        TEST_LOG("------------------------------------------");
        TEST_LOG("Script ", script.name, ": ", failuresFound ? "FAIL" : "PASS", " (", failuresFound, " failures, ", scriptDuration.millisecondsAs<long>(), " ms)");
        TEST_LOG("------------------------------------------");

        totalFailures += failuresFound;
    }

    auto overallDuration = MonotonicTime::now() - overallStartTime;

    TEST_LOG("\n========================================");
    TEST_LOG(totalFailures ? "FAIL" : "PASS", " - Overall Results");
    TEST_LOG("Total Time: ", overallDuration.millisecondsAs<long>(), " ms");
    TEST_LOG("Total Failures: ", totalFailures);
    TEST_LOG("========================================");

    waitForVMCleanupFromPreviousTest();
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
