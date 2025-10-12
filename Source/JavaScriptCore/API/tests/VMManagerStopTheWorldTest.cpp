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
#include "VMManagerStopTheWorldTest.h"

#include "JavaScript.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCConfig.h>
#include <JavaScriptCore/Options.h>
#include <JavaScriptCore/VMManager.h>
#include <string>
#include <thread>
#include <vector>
#include <wtf/Condition.h>
#include <wtf/DataLog.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Scope.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace VMManagerStopTheWorldTest {

using JSC::JSGlobalObject;
using JSC::Options;
using JSC::StopTheWorldEvent;
using JSC::StopTheWorldStatus;
using JSC::VM;
using JSC::VMManager;

/*
 The Stop the World (STW) feature basically involves multiple threads:
 1. Mutator threads running VMs
 2. An optional agent thread (like GC or a Debugger)

 The Actors
 ==========
 This test exercises the Stop the World feature using the following VMs / threads:
 1. main thread
    - this is the conductor that orchesrates the test.
    - see test().
 2. worker threads
    - there are numberOfTestVMs of these, and they each run a VM.
    - each worker will run through a series of checkpoints, which will serve as
      synchronization points for the test.
    - see checkpointCallback(), scriptString(), and inactiveWorkerScriptString().
 3. inactiveVM thread
    - this is a thread with a VM that is not executing some of the time
      i.e. the thread is executing code outside of the VM
      a.k.a. the thread has not entered the VM
      a.k.a. the VM is not activated, hence "inactive".
    - the purpose of having this inactiveVM is to ensure that the existence
      of an deactivated VM does not block active VMs from reaching world stopped mode.
    - we also the scenario where this inactiveVM gets activate while the world is
      already stopped. In such a case, the inactiveVM should block on VM entry and
      not actually execute any code in the VM.
 4. extraVM thread
    - this is a thread that creates a new VM after we've in Stop the World.
      This tests that the new VM will block on VM construction. We need this because
      GlobalGC needs heap activity to stop while the world is stopped. During VM
      construction, a lot of JS objects are allocated (i.e. heap activity). Hence,
      we need to test that VM construction is blocked while the world is stopped.

 The Timeline and Script
 =======================
 All the test scenarios are outlined in the STEPs below. The way to think of the
 STEPs is that each STEP represents a point in time. The orchestration of the test
 may bounce around between the different threads above, and sometimes, more than
 one thread may be running at the same time. However, the execution of all the
 threads are structured so that they obey the STEPS. Depending on the current STEP,
 each thread will perform different work / tests (as outlined below).

 Lastly, this test employs 2 agents: a WasmDebugger and a MemoryDebugger. These are
 only in debuggers in name. We're using them because VMManager provide hooks to
 customize the callbacks for these debuggers.
 - see wasmDebuggerTestCallback() and memoryDebuggerTestCallback().

 In this test, the test WasmDebugger will exercise STW feature like the real
 WasmDebugger would. This include allowing a single VM to run in RunOne mode while
 all other VMs (and their threads) are stopped.

 In this test, the test MemoryDebugger will behave like the GlobalGC agent. The key
 scenario we want to test with it is one where the WasmDebugger has a VM running in
 RunOne mode, and a GC is triggered. This means that the test MemoryDebugger
 (representing GlobalGC) needs to be able to Stop the World while we're in RunOne
 mode, and after it is done, it can resume automatically back in RunOne mode allowing
 the WasmDebugger to continue.

 We will run through the STEPS numberOfIterationsToRun times to ensure that the same
 STW operations can be performed more than once, and that there are no residual state
 that interferes with subsequent operation.

 Outline of the STEPs (test script)
 ==================================
 Setup and Initialization
    STEP 0000 Record VMs pre-existing before this test
    STEP 0001 Start and count inactive workers
    STEP 0001 Start workers
    STEP 0003 Wait till worker threads arrive @ checkpoint 0, and are ready to run tests
    STEP 0004 All workers arrived at Checkpoint 0.
    STEP 0005 Record worker VMs

 Run tests
    STEP 1000 Start test iteration
    STEP 1000.1 Wake all workers
    STEP 1000.2 Wait for workers to arrive at Checkpoint 1

    // Test 1: Stop the World.
    STEP 1100 All workers arrived at Checkpoint 1
    STEP 1100.1 Wake all workers
    STEP 1100.2 Request Stop the World
    STEP 1100.3 Wait for WasmDebugger to stop at Checkpoint 1
    STEP 1190 Success: Stopped in WasmDebugger

    // Test 2: While in StopTheWorld, test that new VM will stop at VM construction.
    STEP 1200 Start Test 2
    STEP 1200.1 All workers have stopped in WasmDebugger
    STEP 1200.2 Start a new thread and confirm that it stops at VM construction
    STEP 1250 Wait for WasmDebugger to detect new thread
    STEP 1290 Success: Found new stopped VM / thread

    // Test 3: While in StopTheWorld, activating the inactive thread should stop at entry.
    STEP 1300 Start Test 3
    STEP 1300.1 Activate the inactive VM
    STEP 1350 Wait for WasmDebugger to detect new thread
    STEP 1390 Success: Found new stopped VM / thread

    // Test 4: Context switch between VMs.
    STEP 1400 Start Test 4
    STEP 1400.1 Switching to a targetVM
    STEP 1490 Success: Context switched thru all test VMs

    // Test 5: RunOne mode in a targetVM thread.
    STEP 1500 Start Test 5
    STEP 1510 RunOne mode initiated
    STEP 1590 Success: TargetVM reached Checkpoint2
    
    // Test 6: While in RunOne mode, another STW request (MemoryDebugger) should succeed.
    STEP 1500 Start Test 6
    STEP 1690 Success: TargetVM reached MemoryDebugger

    // Test 7: MemoryDebugger's Resume should return to RunOne mode.
    STEP 1700 Start Test 7
    STEP 1710 Resumed from MemoryDebugger
    STEP 1790 Success: MemoryDebugger resumed RunOne mode

    // Test 8: While in StopTheWorld, thread completion should ResumeAll
    STEP 1800 Start Test 8
    STEP 1810 RunOne thread breaks out of Checkpoint2
    STEP 1820 RunOne thread will exit imminently
    STEP 1830 World has automatically resumed RunAll mode
    STEP 1890 Success: all threads parked after resuming RunAll

    // Test 9: VM deactivation on RunOne thread should automatically ResumeAll.
    STEP 1900 Start Test 9
    STEP 1910 Request Stop the World
    STEP 1920 Stopped in WasmDebugger
    STEP 1930 RunOne in the inactiveVM worker and get it to exit
    STEP 1940 Wait for VM deactivation to resume RunAll from RunOne
    STEP 1990 Success: All workers auro-resumed RunOne VM deactivated

 To find out what each of the test actors are doing for each STEP, search for the
 corresponding 4 digit STEP number in the code below.
*/

// Debugging options
constexpr int verboseLevel = 0; // 0 = most quiet, 1 = print SET_STEP, 2 = print all.
constexpr bool crashOnAbort = false; // Crash on first failure if true.
constexpr bool logVMManagerInfoOnEachStep = false; // print VMManager::info() on each STEP if true.
constexpr double timeoutIfTestIsUnresponsive = true;

// Test configuration
constexpr unsigned numberOfInactiveVMs = 1; // Must be 1. Do not change.
constexpr unsigned numberOfTestVMs = 5;
constexpr unsigned numberOfIterationsToRun = 3;

// Test runtime state
unsigned step = 0;
bool needAbort = false;
int failuresFound = 0;
Lock lock;
Condition mainThreadConditionVariable;
Condition extraVMConditionVariable;
Condition workersConditionVariable;
Condition inactiveWorkersTerminationConditionVariable;

bool needToNotifyVMDestruction = false;
bool mainIsWaitingForVMDestruction = false;
Condition okToNotifyVMDestructionConditionVariable;
Condition vmDestructionConditionVariable;

Atomic<unsigned> inactiveVMsCreated;
Vector<VM*>* testVMsPtr = nullptr;

bool isCreatingInactiveVM = false;

Atomic<unsigned> totalNumberOfVMs = 0;
unsigned totalNumberOfActiveVMs = 0;
Atomic<unsigned> numberOfThreadsStarted = 0;

namespace Test0 {
Atomic<unsigned> totalNumberOfVMsReady = 0;
}

namespace Test1 {
Atomic<unsigned> numberOfVMsReady = 0;
}

namespace Test2 {
bool reachedCheckpoint0 = false;
unsigned numberOfStoppedVMsAtStart = 0;
VM* extraVM = nullptr;
}

namespace Test3 {
bool reachedCheckpoint5 = false;
unsigned numberOfStoppedVMsAtStart = 0;
}

namespace Test4 {
unsigned numberOfContextSwitches = 0;
VM* targetVM = nullptr;
}

namespace Test5 {
VM* targetVM = nullptr;
}

namespace Test8 {
VM* targetVM = nullptr;
Atomic<unsigned> numberOfRunningThreads = 0;
Atomic<unsigned> numberOfWaitingThreads = 0;
}

namespace Test9 {
VM* targetVM = nullptr;
Atomic<unsigned> numberOfWaitingThreads = 0;
}

namespace TestEnd {
bool doneTesting = false;
}

#undef TID
#undef DLOG
#undef PAD
#undef LOG_STEP_IMPL
#undef LOG_STEP
#undef LOG_INFO
#undef SET_STEP
#undef CHECK
#undef EXPECT_EQ
#undef EXPECT_NE
#undef VMID
#undef WAIT_TIMEOUT_S

#define TID "t", Thread::currentSingleton().uid()

#define DLOG(threadNameOrLabel, ...) \
    dataLogLnIf(verboseLevel >= 2, "<", TID, "> " #threadNameOrLabel " @ ", __LINE__, ": ", __VA_ARGS__)

#define PAD(__step) (verboseLevel > 1 && sizeof(#__step) == 5 ? "  " : "")

#define LOG_STEP_IMPL(__doPrint, __step, threadNameOrLabel, ...) do { \
        dataLogLnIf((__doPrint), "STEP ",  #__step, PAD(__step), " <", TID, "> " #threadNameOrLabel, " @ ", __LINE__, ": ", __VA_ARGS__); \
    } while (false)

#define LOG_STEP(__step, threadNameOrLabel, ...) do { \
        LOG_STEP_IMPL(verboseLevel >= 2, __step, threadNameOrLabel, __VA_ARGS__); \
    } while (false)

#define LOG_INFO(__step, threadNameOrLabel) do { \
        auto info = VMManager::info(); \
        LOG_STEP_IMPL(logVMManagerInfoOnEachStep, __step, threadNameOrLabel, "-- info VMs: ", info.numberOfVMs, ", ActiveVMs: ", info.numberOfActiveVMs, ", StoppedVMs ", info.numberOfStoppedVMs, ", mode: ", info.worldMode); \
    } while (false)

#define SET_STEP(__step, threadNameOrLabel, ...) do { \
        LOG_STEP_IMPL(verboseLevel >= 1, __step, threadNameOrLabel, __VA_ARGS__); \
        LOG_INFO(__step, threadNameOrLabel); \
        step = (unsigned)__step; \
    } while (false)

#define CHECK(condition, ...) do { \
        if (!(condition)) { \
            dataLogLn("FAIL: ", #condition, " @ thread<", TID, ">: ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__); \
            failuresFound++; \
        } \
    } while (false)

#define EXPECT_EQ(actual, expected, ...) do { \
        auto expectedValue = expected; \
        decltype(expectedValue) actualValue = actual; \
        if (expectedValue != actualValue) { \
            dataLogLn("FAIL: EXPECT_EQ(", #actual, ", ", #expected, ", ... @ thread<", TID, ">: ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__); \
            dataLogLn("    actual: ", actual); \
            dataLogLn("  expected: ", expected); \
            failuresFound++; \
        } \
    } while (false)

#define EXPECT_NE(actual, expected, ...) do { \
        auto expectedValue = expected; \
        decltype(expectedValue) actualValue = actual; \
        if (expectedValue == actualValue) { \
            dataLogLn("FAIL: EXPECT_NE(", #actual, ", ",  #expected, ", ... @ thread<", TID, ">: ", __VA_ARGS__); \
            dataLogLn("    @ " __FILE__, ":", __LINE__); \
            dataLogLn("    actual: ", actual); \
            dataLogLn("  expected: !", expected); \
            failuresFound++; \
        } \
    } while (false)

#define VMID(__vm) \
    "vm<", (__vm).identifier(), ">:", RawPointer(&(__vm))

#define WAIT_TIMEOUT_S (timeoutIfTestIsUnresponsive ? Seconds(1.0) : Seconds(std::numeric_limits<double>::infinity()))

class APIString {
    WTF_MAKE_NONCOPYABLE(APIString);
public:

    APIString(const char* string)
        : m_string(JSStringCreateWithUTF8CString(string))
    {
    }

    APIString(JSGlobalContextRef context, JSValueRef value)
        : m_string(JSValueToStringCopy(context, value, nullptr))
    {
    }

    ~APIString()
    {
        if (m_string)
            SUPPRESS_UNCOUNTED_ARG JSStringRelease(m_string);
    }

    operator JSStringRef() { return m_string; }

private:
    SUPPRESS_UNCOUNTED_MEMBER JSStringRef m_string;
};

static int abortTest(Locker<Lock>&)
{
    needAbort = true;
    DLOG(abort, "abortTest");
    mainThreadConditionVariable.notifyAll();
    workersConditionVariable.notifyAll();
    inactiveWorkersTerminationConditionVariable.notifyAll();
    VMManager::requestResumeAll(VMManager::StopReason::WasmDebugger);
    VMManager::requestResumeAll(VMManager::StopReason::MemoryDebugger);
    RELEASE_ASSERT(!crashOnAbort || !needAbort);
    return -1;
}

static int abortTest()
{
    Locker locker { lock };
    return abortTest(locker);
}

static std::vector<std::thread>& threadsList()
{
    static std::vector<std::thread>* list;
    static std::once_flag flag;
    std::call_once(flag, [] () {
        list = new std::vector<std::thread>();
    });
    return *list;
}

static JSValueRef checkpointCallback(JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);

    RELEASE_ASSERT(argumentCount == 1);
    RELEASE_ASSERT(!*exception);

    unsigned checkpointID = JSValueToInt32(ctx, arguments[0], exception);
    RELEASE_ASSERT(!*exception);

    if (needAbort)
        *exception = JSValueMakeNumber(ctx, 42);

    JSValueRef result = nullptr;
    switch (checkpointID) {
    case 0: { // Checkpoint 0
        if (step >= 1820) {
            // Later tests should rendezvous at later checkpoints.
            result = JSValueMakeBoolean(ctx, false);
            break;
        }

        Locker locker { lock };

        if (step >= 1200 && step <= 1290)
            Test2::reachedCheckpoint0 = true;

        // Have all worker threads wait till the main thread is ready before proceeding.
        auto previous = Test0::totalNumberOfVMsReady.exchangeAdd(1);
        if (previous + 1 == numberOfTestVMs) {
            mainThreadConditionVariable.notifyAll();
            SET_STEP(0004, worker, "All workers arrived at Checkpoint 0");
        }
        workersConditionVariable.wait(lock);

        result = JSValueMakeUndefined(ctx);
        break;
    }
    case 1: { // Checkpoint 1
        if (step == 1000) {
            Locker locker { lock };

            // Make sure all threads have reached checkpoint 1.
            auto previous = Test1::numberOfVMsReady.exchangeAdd(1);
            if (previous + 1 == numberOfTestVMs) {
                mainThreadConditionVariable.notifyAll();
                SET_STEP(1100, worker, "All workers arrived at Checkpoint 1");
            }
            workersConditionVariable.wait(lock);

            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step <= 1290) {
            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step <= 1490) {
            LOG_STEP(1490, worker, "Checkpoint 1: Worker should not run while world is stopped in Test 4");
            abortTest();
            break;
        }
        if (step >= 1510) {
            // Later tests should rendezvous at later checkpoints.
            result = JSValueMakeBoolean(ctx, false);
            break;
        }
        LOG_STEP(9999, worker, "Checkpoint 1: should not reach here");
        abortTest(); // Should not reach here.
        break;
    }
    case 2: { // Checkpoint 2
        if (step == 1510) {
            EXPECT_EQ(Test5::targetVM, &vm, "Checkpoint2 reached from the wrong VM thread");
            if (failuresFound) {
                abortTest();
                break;
            }
        }
        if (step < 1590) {
            Locker locker { lock };
            mainThreadConditionVariable.notifyAll();
            SET_STEP(1590, worker, "Success: TargetVM reached Checkpoint2");
            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step == 1710) {
            Locker locker { lock };
            EXPECT_EQ(VMManager::info().worldMode, VMManager::Mode::RunOne, "Should be RunOne");
            mainThreadConditionVariable.notifyAll();
            SET_STEP(1720, worker, "Confirmed transitioned to RunOne mode at Checkpoint2");
            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step == 1800) {
            SET_STEP(1810, worker, "RunOne thread breaks out of Checkpoint2");
            Test8::targetVM = &vm;
            result = JSValueMakeBoolean(ctx, false);
            break;
        }
        if (step >= 1820) {
            // Later tests should rendezvous at later checkpoints.
            result = JSValueMakeBoolean(ctx, false);
            break;
        }
        result = JSValueMakeBoolean(ctx, true);
        break;
    }
    case 3: { // Checkpoint 3
        if (step == 1810) {
            ASSERT(Test8::targetVM == &vm);
            // This VM is imminently exiting and its thread is terminating. Hence, we need to
            // decrement the expected number of VMs and active VMs by 1.
            totalNumberOfActiveVMs--;
            SET_STEP(1820, worker, "RunOne thread will exit imminently");
            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step >= 1820) {
            // Later tests should rendezvous at later checkpoints.
            result = JSValueMakeBoolean(ctx, false);
            break;
        }
        LOG_STEP(9999, worker, "Checkpoint 1: should not reach here");
        abortTest(); // Should not reach here.
        break;
    }
    case 4: { // Checkpoint 4
        // Fall through to Checkpoint 5's code intentionally so that Checkpoint 4
        // will do the equivalent work of Checkpoint 5 because:
        // 1. the regular worker VMs will execute the regular script that goes through
        //    Checkpoint 4.
        // 2. the inactive worker VM will never go through Checkpoint 3, but will end up
        //    here in Checkpoint 5.
        //
        // Test 8 and Test 9 will rely on this same checkpoint behavior.
        [[fallthrough]];
    }
    case 5: { // Checkpoint 5
        // This part is only for Checkpoint 5. When Checkpoint 4 code falls through to here,
        // step will be at least 1820, and will skip this part.
        if (step >= 1300 && step <= 1390) {
            LOG_STEP(1388.5, worker, "@ Checkpoint 5: Test3::reachedCheckpoint5");
            Test3::reachedCheckpoint5 = true;
            abortTest();
            break;
        }

        // The following is common to Checkpoint 4 and 5.
        if (step == 1820) {
            auto previous = Test8::numberOfRunningThreads.exchangeAdd(1);
            if (previous + 1 == totalNumberOfActiveVMs) {
                SET_STEP(1830, worker, "Success: World automatically resumed RunAll mode");
                WTF::storeLoadFence();
            }
            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step == 1830) {
            Locker locker { lock };
            auto previous = Test8::numberOfWaitingThreads.exchangeAdd(1);
            if (previous + 1 == totalNumberOfActiveVMs) {
                SET_STEP(1890, worker, "Success: all threads parked after resuming RunAll");
                mainThreadConditionVariable.notifyAll();
            }
            workersConditionVariable.wait(lock);

            result = JSValueMakeBoolean(ctx, true);
            break;
        }
        if (step == 1940) {
            if (checkpointID == 5) {
                // Get the "inactiveVM" worker to exit its script and deactivate its VM.
                result = JSValueMakeBoolean(ctx, false);
                break;
            }

            Locker locker { lock };
            auto previous = Test9::numberOfWaitingThreads.exchangeAdd(1);
            if (previous + 1 == totalNumberOfActiveVMs) {
                mainThreadConditionVariable.notifyAll();
                SET_STEP(1990, worker, "Success: All workers auro-resumed RunOne VM deactivated");
            }

            workersConditionVariable.wait(lock);
            break;
        }
        // The "inactiveVM" may end up stuck in the Checkpoint 5 loop. So, make it
        // return false (i.e.stop looping) if we're done.
        result = JSValueMakeBoolean(ctx, !TestEnd::doneTesting);
        break;
    }
    case 6: { // Checkpoint 6
        result = JSValueMakeBoolean(ctx, TestEnd::doneTesting);
        break;
    }
    } // end switch (checkpointID)

    if (needAbort)
        *exception = JSValueMakeNumber(ctx, 42);
    return result;
}

// This function is just to act as a sink for JS variables to make sure that the JITs don't
// optimize them away.
static JSValueRef ensureAliveCallback(JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);
    return JSValueMakeUndefined(ctx);
}

static StopTheWorldStatus wasmDebuggerTestCallback(VM& vm, StopTheWorldEvent)
{
    if (step == 1100) {
        // Test 1: Stop the World test.
        Locker locker { lock };
        SET_STEP(1190, wasmDebugger, "Success: Stopped in WasmDebugger");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }

    // Test 2: While in StopTheWorld, test that new VM will stop at construction.
    if (step <= 1200) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }
    if (step == 1250) {
        auto info = VMManager::info();
        if (info.numberOfStoppedVMs <= Test2::numberOfStoppedVMsAtStart) {
            std::this_thread::yield();
            return STW_CONTINUE();
        }

        Locker locker { lock };
        SET_STEP(1290, wasmDebugger, "Success: Found new stopped VM / thread");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }

    // Test 3: Activated VM should stop.
    if (step < 1300) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }
    if (step == 1350) {
        auto info = VMManager::info();
        if (info.numberOfStoppedVMs <= Test3::numberOfStoppedVMsAtStart) {
            std::this_thread::yield();
            return STW_CONTINUE();
        }

        Locker locker { lock };
        SET_STEP(1390, wasmDebugger, "Success: Found new stopped VM / thread");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }

    // Test 4: Context switch test.
    if (step < 1400) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }
    if (step < 1490) {
        if (Test4::targetVM) {
            if (&vm != Test4::targetVM) {
                SET_STEP(9999, wasmDebugger, "Failed: Context switched did not reach targetVM");
                abortTest();
                return STW_RESUME_ALL();
            }
        }
        if (Test4::numberOfContextSwitches < testVMsPtr->size()) {
            RELEASE_ASSERT(!Test4::numberOfContextSwitches || &vm == Test4::targetVM);
            Test4::targetVM = testVMsPtr->at(Test4::numberOfContextSwitches);
            LOG_STEP(1400.1, wasmDebugger, "Switch [", Test4::numberOfContextSwitches, "] from ", VMID(vm), " to targetVM ", VMID(*Test4::targetVM));
            Test4::numberOfContextSwitches++;
            return STW_CONTEXT_SWITCH(Test4::targetVM);
        }
        RELEASE_ASSERT(&vm == Test4::targetVM);

        Locker locker { lock };
        SET_STEP(1490, wasmDebugger, "Success: All context switches succeeded");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }

    // Test 5: RunOne mode in a targetVM thread.
    if (step < 1500) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }
    if (step < 1510) {
        SET_STEP(1510, wasmDebugger, "RunOne mode initiated targetting ", VMID(*Test5::targetVM));
        return STW_RESUME_ONE(Test5::targetVM);
    }
    RELEASE_ASSERT(step != 1510);
    if (step < 1590) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }

    // Test 9: VM deactivation on RunOne thread should automatically ResumeAll.
    if (step < 1900) {
        SET_STEP(9999, wasmDebugger, "Failed: Should not have stopped in the WasmDebugger");
        abortTest();
        return STW_RESUME_ALL();
    }
    if (step == 1910) {
        Locker locker { lock };
        SET_STEP(1920, wasmDebugger, "Stopped in WasmDebugger");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }
    if (step == 1930) {
        SET_STEP(1940, wasmDebugger, "Wait for VM deactivation to resume RunAll from RunOne");
        return STW_RESUME_ONE(Test9::targetVM);
    }
    if (step < 1990) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }

    return STW_RESUME_ALL();
}

static StopTheWorldStatus memoryDebuggerTestCallback(VM&, StopTheWorldEvent)
{
    // Test 6: While in RunOne mode, another STW request (MemoryDebugger) should succeed.
    if (step == 1600) {
        Locker locker { lock };
        SET_STEP(1690, memoryDebugger, "Success: Stopped in MemoryDebugger");
        mainThreadConditionVariable.notifyAll();
        return STW_CONTINUE();
    }
    if (step < 1700) {
        std::this_thread::yield();
        return STW_CONTINUE();
    }
    if (step == 1700) {
        Locker locker { lock };
        SET_STEP(1710, memoryDebugger, "Resumed from MemoryDebugger");
        return STW_RESUME();
    }

    SET_STEP(9999, memoryDebugger, "Failed: Should not have stopped in the MemoryDebugger");
    abortTest();
    return STW_RESUME_ALL();
}

static void notifyVMDestruction()
{
    Locker locker { lock };
    // Either the main thread reaches the rendezvous first or the worker does.
    //
    // If the main thread is first, then the worker's only job is to tell the
    // main thread that they have sync'ed up and both can move forward.
    //
    // If the worker is first, then it needs to give the main thread time to
    // catch up. Hence, the worker should wait for the main thread in that case.
    if (!mainIsWaitingForVMDestruction) {
        // Worker is first. So, wait.
        okToNotifyVMDestructionConditionVariable.wait(lock);
    }
    vmDestructionConditionVariable.notifyAll();
    needToNotifyVMDestruction = false;
}

static void waitForVMDestruction() WTF_REQUIRES_LOCK(lock)
{
    mainIsWaitingForVMDestruction = true;
    okToNotifyVMDestructionConditionVariable.notifyAll();
    vmDestructionConditionVariable.wait(lock);
    WTF::compilerFence();
    mainIsWaitingForVMDestruction = false;
    WTF::loadLoadFence();
}

static int test()
{
    // Flush any output from previous tests before starting this one. This will make
    // test output read a lot more sensibly since this test uses dataLog which prints
    // to stderr while other tests tend to use stdout.
    fflush(stdout);
    fflush(stderr);

    JSC::Config::configureForTesting();
    WTF::initializeMainThread();
    JSC::initialize();

    auto startTime = MonotonicTime::now();
    dataLogLn("");
    dataLogLn("Starting VMManager StopTheWorld Test");

    auto* originalWasmDebugger = g_jscConfig.wasmDebuggerStopTheWorld;
    auto* originalMemoryDebugger = g_jscConfig.memoryDebuggerStopTheWorld;
    VMManager::setWasmDebuggerCallback(wasmDebuggerTestCallback);
    VMManager::setMemoryDebuggerCallback(memoryDebuggerTestCallback);

    // FIXME: for now, VMTraps doesn't completely work on JIT runs yet. Once we fix that, we'll need
    // to upgrade these tests to explicitly trigger and test JIT cases.
    StringBuilder savedOptionsBuilder;
    Options::dumpAllOptionsInALine(savedOptionsBuilder);
    Options::setOptions("--useBaselineJIT=false");

    auto resetSettings = makeScopeExit([&] {
        Options::setOptions(savedOptionsBuilder.toString().ascii().data());
        VMManager::setWasmDebuggerCallback(originalWasmDebugger);
        VMManager::setMemoryDebuggerCallback(originalMemoryDebugger);
    });

#define ABORT_IF_FAILED(__locker) do { \
        if (failuresFound) { \
            abortTest(__locker); \
            return true; \
        } \
    } while (false)

    // Code for worker threads.
    auto task = [] (VM** addressOfVMToExport) {
        int ret = 0;
        // Script for regular worker.
        std::string scriptString = R"script({
            // For active VM workers.
            function foo() { return 1; }
            function bar() { return 2; }
            function baz() { return 3; }

            checkpoint(0);

            // Run the whole thing more than once (controlled by checkpoint(3)) so that we can verify
            // that there's no stuck state between all the stop and resumes.
            for (;;) {
                var x = 0;
                while (checkpoint(1))
                    x += foo();

                while (checkpoint(2))
                    x += bar();

                if (checkpoint(3))
                    break;

                while (checkpoint(4))
                    x += baz();

                if (checkpoint(6))
                    break;
            }
            ensureAlive(x);
        })script";

        // Test 8: The inactive worker creates the worker and then just waits.
        // Script for the "inactiveVM" worker.
        std::string inactiveWorkerScriptString = R"script({
            // For the inactive VM which we later activate.
            function foo() { return 1; }
            var x = 0;
            while (checkpoint(5))
                x += foo();
            ensureAlive(x);
        })script";

        bool isInactiveWorker = isCreatingInactiveVM;

        if (isInactiveWorker)
            LOG_STEP(0001.1, worker, "START thread");
        else {
            numberOfThreadsStarted.exchangeAdd(1);
            LOG_STEP(0002.1, worker, "START thread");
        }

        totalNumberOfVMs.exchangeAdd(1); // JSGlobalContextCreateInGroup will create a VM.
        JSGlobalContextRef context = JSGlobalContextCreateInGroup(nullptr, nullptr);
        JSObjectRef globalObject = JSContextGetGlobalObject(context);
        RELEASE_ASSERT(JSValueIsObject(context, globalObject));
        VM& vm = toJS(context)->vm();
        if (addressOfVMToExport)
            *addressOfVMToExport = &vm;

        if (isInactiveWorker)
            LOG_STEP(0001.2, worker, "Created ", VMID(vm));
        else
            LOG_STEP(0002.2, worker, "Created ", VMID(vm));

        auto installGlobalFunction = [&] (const char* name, auto callback) {
            APIString nameStr(name);
            JSObjectRef function = JSObjectMakeFunctionWithCallback(context, nameStr, callback);
            SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, nameStr, function, kJSPropertyAttributeNone, nullptr);
        };

        installGlobalFunction("checkpoint", checkpointCallback);
        installGlobalFunction("ensureAlive", ensureAliveCallback);

        while (!TestEnd::doneTesting) {
            if (isInactiveWorker) {
                // Initially, the inactive worker creates the worker and then just waits.
                Locker locker { lock };
                auto previous = inactiveVMsCreated.exchangeAdd(1);
                LOG_STEP(0001.3, worker, "previous ", previous, " inactiveVMsCreated ", inactiveVMsCreated.loadRelaxed());
                if (previous + 1 == numberOfInactiveVMs)
                    mainThreadConditionVariable.notifyAll();

                if (TestEnd::doneTesting)
                    break;

                inactiveWorkersTerminationConditionVariable.wait(lock);
            }

            std::string* scriptStringToEvaluate = isInactiveWorker ? &inactiveWorkerScriptString : &scriptString;
            APIString jsScriptString(scriptStringToEvaluate->c_str());
            CHECK(jsScriptString, "script C string to jsString");

            JSValueRef exception = nullptr;
            SUPPRESS_UNCOUNTED_ARG JSValueRef jsScript = JSEvaluateScript(context, jsScriptString, nullptr, nullptr, 0, &exception);
            CHECK(!exception, "unexpected exception from script evaluation");
            if (exception) {
                APIString string(context, exception);
                if (string) {
                    SUPPRESS_UNCOUNTED_ARG Vector<char> buffer(JSStringGetMaximumUTF8CStringSize(string));
                    SUPPRESS_UNCOUNTED_ARG JSStringGetUTF8CString(string, buffer.mutableSpan().data(), buffer.size());
                    dataLogLn("FAIL: thread<", TID, "> ", __FILE__, ":", __LINE__, ": ", buffer.span().data());
                } else
                    dataLogLn("FAIL: thread<", TID, "> ", __FILE__, ":", __LINE__, ": stringifying exception failed");
            }

            CHECK(jsScript, "script evaluation");

            if (step >= 1800 && step < 1890) {
                // In Test 8, the RunOne thread should terminate.
                break; // Don't loop again.
            }
        }

        JSGlobalContextRelease(context);
        totalNumberOfVMs.exchangeSub(1); // JSGlobalContextRelease will destroy the VM.

        if (needToNotifyVMDestruction)
            notifyVMDestruction();

        return ret;
    };

    UncheckedKeyHashSet<VM*> preexistingVMs;
    Vector<VM*> testVMs;
    VM* inactiveVM = nullptr;

    // === Set up and prepare for testing ===================================================

    // Setup initial conditions.
    // If we ever want to run this test more than once (for some internal debugging),
    // explicitly re-initializing to their expected default values will be essential.
    inactiveVMsCreated.exchange(0);
    testVMsPtr = &testVMs; // Hack so that STW callbacks can access this list.
    isCreatingInactiveVM = false;
    numberOfThreadsStarted.exchange(0);
    Test0::totalNumberOfVMsReady.exchange(0);
    Test1::numberOfVMsReady.exchange(0);
    Test2::reachedCheckpoint0 = false;
    TestEnd::doneTesting = false;

    // We should ideally run this test in its own process so that we're in full control of
    // the number of VMs in play. However, for this first cut, we're going to piggy back
    // off of testapi. So, we need to account for pre-existing VMs that may be left over
    // from other tests that testapi runs. We need to track and discount those VMs.

    SET_STEP(0000, main, "Record VMs pre-existing before this test");
    auto error = VMManager::forEachVMWithTimeout(WAIT_TIMEOUT_S, [&] (VM& vm) {
        preexistingVMs.add(&vm);
        return IterationStatus::Continue;
    });
    EXPECT_EQ(error, VMManager::Error::None, "Failed to collect pre-existing VMs.");

    // We expect that no other tests are running concurrently while this test is executing.
    // Hence, the only VMs added/removed should be from this test, and we can track them
    // with some tricks.
    totalNumberOfVMs.exchange(preexistingVMs.size());

    // Start our worker threads.
    SET_STEP(0001, main, "Start and count inactive workers");

    // Start our inactive worker threads.
    isCreatingInactiveVM = true;
    WTF::storeLoadFence();
    for (unsigned t = 0; t < numberOfInactiveVMs; ++t)
        threadsList().push_back(std::thread(task, &inactiveVM));

    if (numberOfInactiveVMs) {
        Locker locker { lock };
        bool workersReady = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!workersReady) {
            CHECK(workersReady, "Not all inactive VM workers were started");
            return abortTest(locker);
        }
    }
    isCreatingInactiveVM = false;
    CHECK(inactiveVM != nullptr, "inactiveVM should be available by now");

    // Start our normal worker threads.
    SET_STEP(0002, main, "Start workers");
    for (unsigned t = 0; t < numberOfTestVMs; ++t)
        threadsList().push_back(std::thread(task, nullptr));

    SET_STEP(0003, main, "Wait till worker threads arrive @ checkpoint 0, and are ready to run tests");
    {
        Locker locker { lock };
        bool workersReady = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!workersReady) {
            CHECK(workersReady, "Not all VM workers were started");
            return abortTest(locker);
        }
    }

    RELEASE_ASSERT(step == 0004);

    // Check that we can see the new number of VM threads created.
    EXPECT_EQ(VMManager::numberOfVMs(), numberOfTestVMs + numberOfInactiveVMs + preexistingVMs.size(), "unexpected number of VMs");
    EXPECT_EQ(numberOfThreadsStarted.loadRelaxed(), numberOfTestVMs, "unexpected number of VMs");

    SET_STEP(0005, main, "Record worker VMs");
    error = VMManager::forEachVMWithTimeout(WAIT_TIMEOUT_S, [&] (VM& vm) {
        if (!preexistingVMs.contains(&vm) && inactiveVM != &vm) {
            LOG_STEP(0005.1, main, "Found test ", VMID(vm));
            testVMs.append(&vm);
        }
        return IterationStatus::Continue;
    });
    {
        Locker locker { lock };
        EXPECT_EQ(error, VMManager::Error::None, "Failed to collect test VMs");
        EXPECT_EQ(testVMs.size(), numberOfTestVMs, "unexpected number of VMs");
        ABORT_IF_FAILED(locker);
    }

    totalNumberOfActiveVMs = testVMs.size();
    LOG_STEP(0005.2, main, "totalNumberOfVMs ", totalNumberOfVMs.loadRelaxed(), " | pre-existing ", preexistingVMs.size(), " inactiveVMs ", numberOfInactiveVMs, " testVMs ", testVMs.size());

    {
        Locker locker { lock };
        auto info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        // info.numberOfActiveVMs is invalid until we have a StopTheWorld request.
        EXPECT_EQ(info.numberOfStoppedVMs, 0, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::RunAll, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);
    }

    // === Ready to run the real tests now ===================================================

    RELEASE_ASSERT(step == 0005);

    for (unsigned iteration = 0; iteration < numberOfIterationsToRun; ++iteration) {
        Locker locker { lock };
        bool ready;
        VMManager::Info info;

        Test1::numberOfVMsReady.exchange(0);
        WTF::storeLoadFence();

        dataLogLnIf(verboseLevel >= 1 && !iteration);
        dataLogLn("=== iteration ", iteration, " START ==============================");
        SET_STEP(1000, main, "Start test iteration [", iteration, "]");

        LOG_STEP(1000.1, main, "Wake all workers");
        workersConditionVariable.notifyAll();

        LOG_STEP(1000.2, main, "Wait for workers to arrive at Checkpoint 1");
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "Not all worker threads reached Checkpoint 1: expect ", numberOfTestVMs, ", actual ", Test1::numberOfVMsReady.loadRelaxed());
            return abortTest(locker);
        }

        EXPECT_EQ(Test1::numberOfVMsReady.loadRelaxed(), numberOfTestVMs, "");

        // === Test 1 ==============================================================
        EXPECT_EQ(step, 1100, "Unexpected step: expect 1100, actual ", step);

        LOG_STEP(1100.1, main, "Wake all workers");
        workersConditionVariable.notifyAll();

        LOG_STEP(1100.2, main, "Request Stop the World");
        VMManager::requestStopAll(VMManager::StopReason::WasmDebugger);

        LOG_STEP(1100.3, main, "Wait for WasmDebugger to stop at Checkpoint 1");
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "WasmDebugger did NOT stop in checkpoint 1 loop");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1190, "unexpected step");

        // === Test 2 ==============================================================
        Test2::extraVM = nullptr;

        SET_STEP(1200, main, "Start Test 2");

        LOG_STEP(1200.1, main, "All workers have stopped in WasmDebugger");
        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "Unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "Unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "Unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        Test2::reachedCheckpoint0 = false;
        Test2::numberOfStoppedVMsAtStart = VMManager::info().numberOfStoppedVMs;

        LOG_STEP(1200.2, main, "Start a new thread and confirm that it stops at VM construction");
        threadsList().push_back(std::thread(task, &Test2::extraVM));
        WTF::storeStoreFence();

        SET_STEP(1250, main, "Wait for WasmDebugger to detect new thread");
        WTF::storeStoreFence();
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "WasmDebugger did NOT detect new thread");
            return abortTest(locker);
        }

        EXPECT_EQ(step, 1290, "unexpected step");

        CHECK(Test2::extraVM == nullptr, "Should have blocked at VM construction and not set Test2::extraVM yet");

        totalNumberOfActiveVMs++;

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "unexpected VMManager mode");
        EXPECT_EQ(Test2::reachedCheckpoint0, false, "new VM did not stop on construction");
        ABORT_IF_FAILED(locker);

        // === Test 3 ==============================================================
        Test3::reachedCheckpoint5 = false;
        Test3::numberOfStoppedVMsAtStart = VMManager::info().numberOfStoppedVMs;
        WTF::storeLoadFence();

        SET_STEP(1300, main, "Start Test 3");

        LOG_STEP(1300.1, main, "Activate the inactive VM");
        inactiveWorkersTerminationConditionVariable.notifyAll();

        SET_STEP(1350, main, "Wait for WasmDebugger to detect new thread");
        WTF::storeStoreFence();
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "WasmDebugger did NOT detect new thread");
            return abortTest(locker);
        }

        EXPECT_EQ(step, 1390, "unexpected step");

        // totalNumberOfActiveVMs has increased by 1 because of the new thread we just activated.
        // Though the thread would have stopped at VM entry, it counts as active.
        totalNumberOfActiveVMs++;

        info = VMManager::info();
        LOG_STEP(1390.1, main, "Test3::reachedCheckpoint5 ", Test3::reachedCheckpoint5);
        LOG_STEP(1390.1, main, "Test3::numberOfStoppedVMsAtStart ", Test3::numberOfStoppedVMsAtStart);
        LOG_STEP(1390.1, main, "info.numberOfVMs ", info.numberOfVMs, " totalNumberOfVMs ", totalNumberOfVMs.loadRelaxed());
        LOG_STEP(1390.1, main, "info.numberOfActiveVMs ", info.numberOfActiveVMs, " totalNumberOfActiveVMs ", totalNumberOfActiveVMs);
        LOG_STEP(1390.1, main, "info.numberOfStoppedVMs ", info.numberOfStoppedVMs, " totalNumberOfActiveVMs ", totalNumberOfActiveVMs);

        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "unexpected VMManager mode");
        EXPECT_EQ(Test3::reachedCheckpoint5, false, "Activated VM did not stop on entry");
        ABORT_IF_FAILED(locker);

        // === Test 4 ==============================================================
        Test4::numberOfContextSwitches = 0;
        Test4::targetVM = nullptr;
        WTF::storeStoreFence();

        SET_STEP(1400, main, "Start Test 4");
        WTF::storeStoreFence();
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "Context switch test did not complete");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1490, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        // === Test 5 ==============================================================
        EXPECT_NE(Test4::targetVM, testVMs.at(0), "WasmDebugger should have context switched away from the 0th test VM");
        ABORT_IF_FAILED(locker);

        Test5::targetVM = testVMs.at(0); // Let's do RunOne mode with a context switch.
        WTF::storeStoreFence();

        SET_STEP(1500, main, "Start Test 5");
        WTF::storeStoreFence();
        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "RunOne mode in targetVM did not reach Checkpoint 2");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1590, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs - 1, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::RunOne, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        // === Test 6 ==============================================================
        SET_STEP(1600, main, "Start Test 6");
        WTF::storeStoreFence();
        VMManager::requestStopAll(VMManager::StopReason::MemoryDebugger);

        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "Did not stop in MemoryDebugger");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1690, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        // === Test 7 ==============================================================
        SET_STEP(1700, main, "Start Test 7");
        WTF::storeStoreFence();

        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "MemoryDebugger did not resume");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1720, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of active VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs - 1, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::RunOne, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        SET_STEP(1790, main, "Success: MemoryDebugger resumed RunOne mode");

        // === Test 8 ==============================================================
        needToNotifyVMDestruction = true;
        Test8::targetVM = nullptr;
        Test8::numberOfRunningThreads.exchange(0);
        Test8::numberOfWaitingThreads.exchange(0);
        WTF::storeStoreFence();

        SET_STEP(1800, main, "Start Test 8");
        WTF::storeStoreFence();

        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "World did not ResumeAll");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1890, "unexpected step");
        CHECK(Test2::extraVM != nullptr, "Failed to create extra VM");

        // Note: we already decremented totalNumberOfVMs and totalNumberOfActiveVMs for
        // the exiting thread back in STEP 1810 in Checkpoint 3.

        waitForVMDestruction(); // totalNumberOfVMs should be accurate after this.

        // Fix up the testVMs list now that Test5::targetVM has exited.
        RELEASE_ASSERT(Test5::targetVM == testVMs.at(0));
        testVMs.at(0) = Test2::extraVM; // Replace the terminated VM with the extra VM.
        Test2::extraVM = nullptr;
        Test5::targetVM = nullptr;

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        // We have ResumeAll at this point i.e. we're no longer in StopTheWorld. Hence,
        // info.numberOfActiveVMs is invalid.
        EXPECT_EQ(info.numberOfStoppedVMs, 0, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::RunAll, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        // === Test 9 ==============================================================
        Test9::targetVM = inactiveVM;
        RELEASE_ASSERT(Test9::targetVM);
        Test9::numberOfWaitingThreads.exchange(0);
        WTF::storeLoadFence();

        SET_STEP(1900, main, "Start Test 9");
        workersConditionVariable.notifyAll(); // Set the workers free.

        SET_STEP(1910, main, "Request Stop the World");
        VMManager::requestStopAll(VMManager::StopReason::WasmDebugger);

        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "WasmDebugger did NOT stop at checkpoints 4 and 5");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1920, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        EXPECT_EQ(info.numberOfActiveVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.numberOfStoppedVMs, totalNumberOfActiveVMs, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::Stopped, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        Test9::numberOfWaitingThreads.exchange(0);
        WTF::storeLoadFence();

        SET_STEP(1930, main, "RunOne in the inactiveVM worker and get it to exit");
        // The number of active VMs won't actual decrement until the inactiveVM exits and
        // deactivates. However, no one will use the value in totalNumberOfActiveVMs until
        // step 1940. So, we'll pre-emptively decrement it as there's no other convenience
        // place to decrement this.
        totalNumberOfActiveVMs--;

        ready = mainThreadConditionVariable.waitFor(lock, WAIT_TIMEOUT_S);
        if (!ready) {
            CHECK(ready, "WasmDebugger did NOT auto-resume RunAll after VM deactivation");
            return abortTest(locker);
        }
        EXPECT_EQ(step, 1990, "unexpected step");

        info = VMManager::info();
        EXPECT_EQ(info.numberOfVMs, totalNumberOfVMs.loadRelaxed(), "unexpected number of VMs");
        // We have ResumeAll at this point i.e. we're no longer in StopTheWorld. Hence,
        // info.numberOfActiveVMs is invalid.
        EXPECT_EQ(info.numberOfStoppedVMs, 0, "unexpected number of stopped VMs");
        EXPECT_EQ(info.worldMode, VMManager::Mode::RunAll, "unexpected VMManager mode");
        ABORT_IF_FAILED(locker);

        // Test Loop End: Prepare to run another iteration or exit.
        dataLogLn("=== iteration ", iteration, " END ================================");
        dataLogLnIf(verboseLevel >= 1, "");

        if (iteration < numberOfIterationsToRun - 1) {
            TestEnd::doneTesting = false;
            step = 0005; // Reset step to run next test iteration
        } else
            TestEnd::doneTesting = true;

        workersConditionVariable.notifyAll();
    }

    // === Shutting down ===================================================
    {
        Locker locker { lock };
        workersConditionVariable.notifyAll();
        inactiveWorkersTerminationConditionVariable.notifyAll();
    }

    DLOG(main, "Waiting for workers to shut down");
    auto& threads = threadsList();
    for (auto& thread : threads)
        thread.join();
    threads.clear();

    auto endTime = MonotonicTime::now();

    dataLogLn(failuresFound ? "FAIL"_s : "PASS"_s, " VMManager StopTheWorld Test (running time: ", (endTime - startTime).millisecondsAs<long>(), " ms)");
    return (failuresFound > 0);

#undef ABORT_IF_FAILED
}

#undef TID
#undef DLOG
#undef PAD
#undef LOG_STEP_IMPL
#undef LOG_STEP
#undef LOG_INFO
#undef SET_STEP
#undef CHECK
#undef EXPECT_EQ
#undef EXPECT_NE
#undef VMID
#undef WAIT_TIMEOUT_S

} // namespace VMManagerStopTheWorldTest

int testVMManagerStopTheWorld()
{
    return VMManagerStopTheWorldTest::test();
}
