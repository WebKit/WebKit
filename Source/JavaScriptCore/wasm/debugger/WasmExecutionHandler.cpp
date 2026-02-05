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
#include "WasmExecutionHandler.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "Options.h"
#include "SubspaceInlines.h"
#include "VM.h"
#include "VMManager.h"
#include "WasmBreakpointManager.h"
#include "WasmCallee.h"
#include "WasmDebugServer.h"
#include "WasmDebugServerUtilities.h"
#include "WasmIPIntGenerator.h"
#include "WasmIPIntSlowPaths.h"
#include "WasmModuleManager.h"
#include "WasmOps.h"
#include "WasmVirtualAddress.h"
#include <cstdlib>
#include <cstring>
#if OS(WINDOWS)
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <wtf/Assertions.h>
#include <wtf/DataLog.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExecutionHandler);

struct StopReasonInfo {
    String reasonString;
    StringView reasonSuffix;
};

static inline StopReasonInfo stopReasonCodeToInfo(StopData::Code code)
{
    switch (code) {
    case StopData::Code::Stop:
#if OS(DARWIN)
        return { "T11"_s, "signal"_s }; // SIGSTOP on macOS (17 decimal = 0x11 hex)
#elif OS(LINUX)
        return { "T13"_s, "signal"_s }; // SIGSTOP on Linux (19 decimal = 0x13 hex)
#else
        return { "T11"_s, "signal"_s }; // Default: SIGSTOP (most common value)
#endif
    case StopData::Code::Trace:
        return { "T05"_s, "trace"_s }; // SIGTRAP - Trace/single step
    case StopData::Code::Breakpoint:
        return { "T05"_s, "breakpoint"_s }; // SIGTRAP - Breakpoint hit
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { String(), "trace"_s };
    }
}

ExecutionHandler::ExecutionHandler(DebugServer& debugServer, ModuleManager& instanceManager)
    : m_debugServer(debugServer)
    , m_moduleManager(instanceManager)
    , m_breakpointManager(makeUnique<BreakpointManager>())
{
}

void ExecutionHandler::stopTheWorld(VM& debuggee, StopTheWorldEvent event)
{
    auto info = VMManager::info();
    dataLogLnIf(Options::verboseWasmDebugger(), "[stopTheWorld] debuggee:", RawPointer(&debuggee), " event:", event, " ", info);

    {
        Locker locker { m_lock };

        switch (event) {
        case StopTheWorldEvent::StepIntoSiteReached:
            RELEASE_ASSERT(Thread::currentSingleton().uid() == threadId(*m_debuggee));
            RELEASE_ASSERT(m_debuggee == info.targetVM && info.worldMode == VMManager::Mode::RunOne);
            break;
        case StopTheWorldEvent::BreakpointHit:
            RELEASE_ASSERT(info.worldMode != VMManager::Mode::Stopped);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        // Wait for previous resume all to prevent reenter notifyVMStop.
        while (m_awaitingResumeNotification)
            m_debuggeeContinue.wait(m_lock);
        m_debuggee = &debuggee;
    }

    VMManager::requestStopAll(VMManager::StopReason::WasmDebugger);
    VMManager::singleton().notifyVMStop(debuggee, event);
}

bool ExecutionHandler::hitBreakpoint(CallFrame* callFrame, JSWebAssemblyInstance* instance, IPIntCallee* callee, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* locals, IPInt::IPIntStackEntry* stack)
{
    VirtualAddress address = VirtualAddress::toVirtual(instance, callee->functionIndex(), pc);
    if (auto* breakpoint = m_breakpointManager->findBreakpoint(address)) {
        VM& debuggee = instance->vm();
        debuggee.debugState()->setBreakpointStopData(breakpoint->type, address, breakpoint->originalBytecode, pc, mc, locals, stack, callee, instance, callFrame);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][hitBreakpoint] Going to stop at ", *breakpoint, " with ", *debuggee.debugState()->stopData);
        stopTheWorld(debuggee, StopTheWorldEvent::BreakpointHit);
        return true;
    }
    return false;
}

ExecutionHandler::ResumeMode ExecutionHandler::stopCode(Locker<Lock>& locker, StopTheWorldEvent event)
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == threadId(*m_debuggee));

    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Start with event:", event);

    debuggeeState()->setStopped();

    auto notifyDebuggerOfStop = [&]() WTF_REQUIRES_LOCK(m_lock) {
        if (m_debuggerState == DebuggerState::ContinueRequested) {
            sendStopReply(locker);
            dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Sent a stop reply and waiting...");
        } else {
            RELEASE_ASSERT(requiresStopConfirmation());
            m_debuggerContinue.notifyOne(); // Notify debugger that code stopped.
        }
    };

    // Handle stop event and notify debugger
    switch (event) {
    case StopTheWorldEvent::VMStopped:
    case StopTheWorldEvent::VMCreated:
    case StopTheWorldEvent::VMActivated:
        RELEASE_ASSERT(m_debuggerState == DebuggerState::InterruptRequested || m_debuggerState == DebuggerState::SwitchRequested);
        notifyDebuggerOfStop();
        break;
    case StopTheWorldEvent::BreakpointHit:
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested || m_debuggerState == DebuggerState::ContinueRequested || m_debuggerState == DebuggerState::SwitchRequested);
        m_breakpointManager->clearAllOneTimeBreakpoints();
        notifyDebuggerOfStop();
        break;
    case StopTheWorldEvent::StepIntoSiteReached:
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested);
        m_debuggerContinue.notifyOne(); // Notify that breakpoint is set.
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Wait for debugger command
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Waiting for debugger command...");
    m_debuggeeContinue.wait(locker); // Wait for resume mode to be set.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Unblocked and running...");

    // Determine resume mode
    if (m_debuggerState == DebuggerState::SwitchRequested)
        return ResumeMode::Switch;

    debuggeeState()->clearStop();

    // Defer debugger notification until after VMs resume to prevent interrupt() race.
    if (m_debuggerState == DebuggerState::ContinueRequested)
        m_awaitingResumeNotification = true;

    return (m_debuggerState == DebuggerState::StepRequested) ? ResumeMode::One : ResumeMode::All;
}

StopTheWorldStatus ExecutionHandler::handleStopTheWorld(VM& debuggee, StopTheWorldEvent event)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[STW] handleStopTheWorld() begin with event:", event, " debuggee:", RawPointer(&debuggee));
    Locker locker { m_lock };

    selectDebuggeeIfNeeded(debuggee);
    RELEASE_ASSERT(m_debuggee);

    // If debuggee differs from candidate, switch to debuggee.
    if (m_debuggee != &debuggee) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Context switch to debuggee VM");
        return STW_CONTEXT_SWITCH(m_debuggee);
    }

    // Mark VMs' states (debuggee running, others stopped).
    markVMStates(m_debuggee);

    // Stop the code and let debugger decide resume mode.
    ExecutionHandler::ResumeMode mode = stopCode(locker, event);
    switch (mode) {
    case ExecutionHandler::ResumeMode::One:
        RELEASE_ASSERT(m_debuggee->debugState()->isRunning());
        return STW_RESUME_ONE(m_debuggee);
    case ExecutionHandler::ResumeMode::All:
        RELEASE_ASSERT(m_debuggee->debugState()->isRunning());
        clearOtherVMStopData(m_debuggee);
        return STW_RESUME_ALL();
    case ExecutionHandler::ResumeMode::Switch:
        RELEASE_ASSERT(m_debuggee != &debuggee);
        RELEASE_ASSERT(debuggee.debugState()->isStopped());
        RELEASE_ASSERT(m_debuggee->debugState()->isStopped());
        debuggee.debugState()->setRunning();
        m_debuggee->debugState()->setRunning();
        return STW_CONTEXT_SWITCH(m_debuggee);
    }
    return STW_RESUME_ALL();
}

void ExecutionHandler::selectDebuggeeIfNeeded(VM& fallbackVM) WTF_REQUIRES_LOCK(m_lock)
{
    if (m_debuggee) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Already have debuggee VM:", RawPointer(m_debuggee));
        return;
    }

    // Prefer VM at prologue, otherwise use the triggered VM
    VM* selectedVM = nullptr;
    VMManager::forEachVM([&](VM& vm) {
        auto* debugState = vm.debugState();
        if (debugState && debugState->atPrologue()) {
            selectedVM = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });

    m_debuggee = selectedVM ? selectedVM : &fallbackVM;
    dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Selected debuggee VM:", RawPointer(m_debuggee));
}

void ExecutionHandler::markVMStates(VM* debuggee) WTF_REQUIRES_LOCK(m_lock)
{
    VMManager::forEachVM([&](VM& vm) {
        if (&vm == debuggee)
            vm.debugState()->setRunning();
        else
            vm.debugState()->setStopped();
        return IterationStatus::Continue;
    });
}

void ExecutionHandler::clearOtherVMStopData(VM* debuggee) WTF_REQUIRES_LOCK(m_lock)
{
    VMManager::forEachVM([&](VM& vm) {
        if (&vm != debuggee) {
            RELEASE_ASSERT(vm.debugState()->isStopped());
            vm.debugState()->clearStop();
        }
        return IterationStatus::Continue;
    });
}

// Called by VMManager when ALL VMs are stopped at safe points.
// Returns resume mode (ResumeOne/ResumeAll/ContextSwitch) to coordinate execution.
StopTheWorldStatus wasmDebuggerOnStopCallback(VM& debuggee, StopTheWorldEvent event)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Callback invoked with event:", event);
    auto& server = DebugServer::singleton();
    if (!server.isConnected()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Not connected, resuming all");
        return STW_RESUME_ALL();
    }

    return server.execution().handleStopTheWorld(debuggee, event);
}

void ExecutionHandler::handlePostResume()
{
    Locker locker { m_lock };

    if (takeAwaitingResumeNotification()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[PostResume] Notify debugger to continue");
        m_debuggerContinue.notifyOne(); // Notify that resume is complete.
        m_debuggeeContinue.notifyAll(); // Release resume barrier for VMs blocked in stopTheWorld().
    }
}

// Called by VMManager after ALL VMs have resumed (when m_numberOfStoppedVMs reaches 0).
// This is the safe point to notify debugger that resume is complete and release barriers in stopTheWorld().
void wasmDebuggerOnResumeCallback()
{
    auto& server = DebugServer::singleton();
    if (!server.isConnected()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[STW][PostResume] Not connected, resuming all");
        return;
    }

    server.execution().handlePostResume();
}

void ExecutionHandler::resume()
{
    Locker locker { m_lock };
    resumeImpl(locker);
}

void ExecutionHandler::resumeImpl(Locker<Lock>& locker)
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == debugServerThreadId());
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Start");

    RELEASE_ASSERT(debuggeeState()->isStopped());
    m_debuggerState = DebuggerState::ContinueRequested;
    m_debuggeeContinue.notifyOne(); // Notify debuggee VM with resume all command.

    // Wait for resume to complete to prevent interrupt() from acquiring lock before VMs resume.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Notified code to continue and waiting...");
    m_debuggerContinue.wait(locker); // Wait for resume to complete.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Confirmed that code is running...");
}

static inline VM* findVM(uint64_t threadId)
{
    VM* result = nullptr;
    VMManager::forEachVM([&](VM& vm) {
        if (threadId == ExecutionHandler::threadId(vm)) {
            result = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return result;
}

void ExecutionHandler::switchTarget(uint64_t threadId)
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == debugServerThreadId());

    Locker locker { m_lock };

    VM* newDebuggee = findVM(threadId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][SwitchVM] current debuggee=", RawPointer(m_debuggee), " new debuggee=", RawPointer(newDebuggee));

    if (m_debuggee == newDebuggee)
        return;

    RELEASE_ASSERT(debuggeeState()->isStopped());
    m_debuggee = newDebuggee;
    m_debuggerState = DebuggerState::SwitchRequested;
    m_debuggeeContinue.notifyOne(); // Notify to switch VM context.

    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][SwitchVM] Notified code to continue and switch VM, waiting...");
    m_debuggerContinue.wait(locker); // Wait for new debuggee VM to stop.
    RELEASE_ASSERT(debuggeeState()->isStopped());
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][SwitchVM] Code is stopped");
}

void ExecutionHandler::interrupt()
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == debugServerThreadId());

    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Start");

    // LLDB implements interrupt flood prevention: once LLDB sends the first interrupt packet,
    // no matter how many Ctrl+C the user types, LLDB will not send additional interrupt packets
    // until it receives a stop reply. This prevents packet flooding and ensures clean protocol behavior.
    // Our WebKit implementation handles each interrupt request by activating StopWorld via VM traps.

    if (m_debuggee && debuggeeState()->isStopped())
        return;

    {
        RELEASE_ASSERT(!m_debuggee || debuggeeState()->isRunning());
        m_debuggerState = DebuggerState::InterruptRequested;
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Calling VMManager::requestStopAll()...");
        VMManager::singleton().requestStopAll(VMManager::StopReason::WasmDebugger);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] VMManager::requestStopAll() returned");
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Notified code to stop, waiting...");
    m_debuggerContinue.wait(locker); // Wait for debuggee VM to stop.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Wait completed, sending stop reply...");
    sendStopReply(locker);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Code is stopped and debugger replied");
}

void ExecutionHandler::step()
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == debugServerThreadId());

    Locker locker { m_lock };
    auto* state = debuggeeState();
    RELEASE_ASSERT(m_debuggerState == DebuggerState::Replied && state->isStopped());

    bool resumeAll = false;
    if (state->atSystemCall())
        resumeAll = true;
    else if (state->atBreakpoint())
        resumeAll = stepAtBreakpoint(locker, state);
    else {
        RELEASE_ASSERT(state->atPrologue());
        setBreakpointAtEntry(state->stopData->instance, state->stopData->callee.get(), Breakpoint::Type::Step);
    }

    if (resumeAll) {
        resumeImpl(locker);
        return;
    }

    RELEASE_ASSERT(m_breakpointManager->hasOneTimeBreakpoints());
    m_debuggerState = DebuggerState::StepRequested;
    m_debuggeeContinue.notifyOne(); // Notify to resume until next breakpoint.

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Notified code to continue and expected a stop, waiting...");
    m_debuggerContinue.wait(locker); // Wait for one-time breakpoint hit.
    sendStopReply(locker);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Code is stopped and debugger replied");
}

bool ExecutionHandler::stepAtBreakpoint(Locker<Lock>& locker, DebugState* state)
{
    RELEASE_ASSERT(state->atBreakpoint());
    auto& stopData = *state->stopData;
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Start with ", stopData);

    uint8_t* currentPC = stopData.pc;

    auto setStepBreakpoint = [&](const uint8_t* nextPC) WTF_REQUIRES_LOCK(m_lock) {
        VirtualAddress nextAddress = VirtualAddress(stopData.address.value() + (nextPC - currentPC));
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step][SetOneTimeBreakpoint] current PC=", RawPointer(currentPC), "(", stopData.address, "), next PC=", RawPointer(nextPC), "(", nextAddress, ")");
        if (m_breakpointManager->findBreakpoint(nextAddress))
            return;
        m_breakpointManager->setBreakpoint(nextAddress, Breakpoint(const_cast<uint8_t*>(nextPC), Breakpoint::Type::Step));
    };

    auto setStepBreakpointAtCaller = [&]() WTF_REQUIRES_LOCK(m_lock) {
        uint8_t* returnPC = nullptr;
        VirtualAddress virtualReturnPC;
        if (getWasmReturnPC(stopData.callFrame, returnPC, virtualReturnPC))
            m_breakpointManager->setBreakpoint(virtualReturnPC, Breakpoint(const_cast<uint8_t*>(returnPC), Breakpoint::Type::Step));
    };

    auto setStepBreakpointsFromDebugInfo = [&]() WTF_REQUIRES_LOCK(m_lock) {
        const auto& moduleInfo = stopData.instance->moduleInformation();
        auto functionIndex = stopData.callee->functionIndex();
        uint32_t offset = stopData.address.offset();
        const auto* nextInstructions = moduleInfo.debugInfo->ensureFunctionDebugInfo(functionIndex).findNextInstructions(offset);
        RELEASE_ASSERT(nextInstructions, "Didn't find nextInstructions");
        uint8_t* const basePC = stopData.pc - offset;
        for (uint32_t nextOffset : *nextInstructions)
            setStepBreakpoint(basePC + nextOffset);
    };

    switch (stopData.originalBytecode) {
    case Nop:
    case Drop:
    case Select:
        setStepBreakpoint(currentPC + 1);
        break;
    case End:
        if (currentPC != stopData.callee->bytecodeEnd()) {
            setStepBreakpoint(currentPC + 1);
            break;
        }
        [[fallthrough]];
    case Return:
        setStepBreakpointAtCaller();
        break;
    // Step-into design: Set event flag and let runtime resolve callee/handler naturally to avoid duplicating
    // complex resolution logic. Runtime calls setStepIntoBreakpointForCall/Throw() with resolved target.
    // FIXME: Resolve target once in step() and cache for runtime reuse to eliminate two-phase protocol.
    case Throw:
    case Rethrow:
    case ThrowRef:
    case Delegate:
        state->setStepIntoThrow();
        break;
    case TailCall:
    case TailCallIndirect:
    case TailCallRef:
        // Tail calls: Set step-into event for Wasm callees, then set breakpoint at caller
        // (for non-Wasm callees or as fallback, since tail calls don't return to next instruction).
        state->setStepIntoCall();
        setStepBreakpointAtCaller();
        break;
    case Call:
    case CallIndirect:
    case CallRef:
        // Regular calls: Set step-into event for Wasm callees, then fallthrough to
        // set step-over breakpoint at next instruction (for non-Wasm callees or as fallback).
        state->setStepIntoCall();
        [[fallthrough]];
    default: {
        setStepBreakpointsFromDebugInfo();
        break;
    }
    }

    if (state->hasStepIntoEvent()) {
        m_debuggerState = DebuggerState::StepRequested;
        m_debuggeeContinue.notifyOne(); // Notify to run and set breakpoints if possible.
        m_debuggerContinue.wait(locker); // Wait for call/throw one-time breakpoint to be registered.
    }

    // If no one-time breakpoints registered, then resume all.
    return !m_breakpointManager->hasOneTimeBreakpoints();
}

void ExecutionHandler::setStepIntoBreakpointForCall(VM& callerVM, CalleeBits boxedCallee, JSWebAssemblyInstance* calleeInstance)
{
    if (!callerVM.debugState()->takeStepIntoCall())
        return;

    [&]() {
        Locker locker { m_lock };

        RELEASE_ASSERT(Thread::currentSingleton().uid() == threadId(*m_debuggee));
        RELEASE_ASSERT(m_debuggee == &callerVM);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][StepIntoEvent] Start for call");
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested);

        if (!calleeInstance)
            return;
        if (!boxedCallee.isNativeCallee())
            return;
        RefPtr wasmCallee = downcast<Wasm::Callee>(boxedCallee.asNativeCallee());
        if (wasmCallee->compilationMode() != Wasm::CompilationMode::IPIntMode)
            return;

        // Set breakpoint at the callee's entry point.
        // Use calleeInstance (not caller's instance) because callee may be in a different Wasm module instance.
        RELEASE_ASSERT(&calleeInstance->vm() == &callerVM);
        setBreakpointAtEntry(calleeInstance, downcast<IPIntCallee>(wasmCallee.get()), Breakpoint::Type::Step);
    }();

    stopTheWorld(callerVM, StopTheWorldEvent::StepIntoSiteReached);
}

void ExecutionHandler::setStepIntoBreakpointForThrow(VM& throwVM)
{
    if (!throwVM.debugState()->takeStepIntoThrow())
        return;

    [&]() {
        Locker locker { m_lock };

        RELEASE_ASSERT(Thread::currentSingleton().uid() == threadId(*m_debuggee));
        RELEASE_ASSERT(m_debuggee == &throwVM);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][StepIntoEvent] Start for throw");
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested);

        if (!throwVM.callFrameForCatch)
            return;
        if (!throwVM.callFrameForCatch->callee().isNativeCallee())
            return;
        RefPtr wasmCallee = downcast<Wasm::Callee>(throwVM.callFrameForCatch->callee().asNativeCallee());
        if (wasmCallee->compilationMode() != Wasm::CompilationMode::IPIntMode)
            return;

        RefPtr catchCallee = downcast<IPIntCallee>(wasmCallee.get());
        ASSERT(std::holds_alternative<uintptr_t>(throwVM.targetInterpreterPCForThrow));
        uintptr_t handlerOffset = std::get<uintptr_t>(throwVM.targetInterpreterPCForThrow);
        const uint8_t* handlerPC = catchCallee->bytecode() + handlerOffset;

        if (*handlerPC == static_cast<uint8_t>(Wasm::OpType::TryTable) && throwVM.targetInterpreterMetadataPCForThrow) {
            const uint8_t* metadataPtr = catchCallee->metadata() + throwVM.targetInterpreterMetadataPCForThrow;
            metadataPtr += sizeof(IPInt::CatchMetadata);
            const IPInt::BlockMetadata* blockMetadata = reinterpret_cast<const IPInt::BlockMetadata*>(metadataPtr);
            handlerPC = handlerPC + blockMetadata->deltaPC;
        }

        // Set breakpoint at the exception handler.
        // Use catchInstance (not thrower's instance) because exception may be caught in a different Wasm module instance.
        JSWebAssemblyInstance* catchInstance = throwVM.callFrameForCatch->wasmInstance();
        RELEASE_ASSERT(&catchInstance->vm() == &throwVM);
        setBreakpointAtPC(catchInstance, catchCallee->functionIndex(), Breakpoint::Type::Step, handlerPC);
    }();

    stopTheWorld(throwVM, StopTheWorldEvent::StepIntoSiteReached);
}

void ExecutionHandler::setBreakpointAtEntry(JSWebAssemblyInstance* instance, IPIntCallee* callee, Breakpoint::Type type)
{
    setBreakpointAtPC(instance, callee->functionIndex(), type, callee->bytecode());
}

void ExecutionHandler::setBreakpointAtPC(JSWebAssemblyInstance* instance, FunctionCodeIndex functionIndex, Breakpoint::Type type, const uint8_t* pc)
{
    RELEASE_ASSERT(pc);
    VirtualAddress address = VirtualAddress::toVirtual(instance, functionIndex, pc);
    if (m_breakpointManager->findBreakpoint(address))
        return;
    m_breakpointManager->setBreakpoint(address, Breakpoint(const_cast<uint8_t*>(pc), type));
}

void ExecutionHandler::setBreakpoint(StringView packet)
{
    if (packet.isEmpty())
        return;

    // Parse packet format: Z0,<address>,<length>
    if (packet.isEmpty() || packet[0] != 'Z') {
        sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    StringView params = packet.substring(1);
    auto parts = splitWithDelimiters(params, ",,"_s);
    if (parts.size() != 3) {
        sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    uint32_t type = parseDecimal(parts[0]);
    VirtualAddress address = VirtualAddress(parseHex(parts[1]));
    uint32_t length = parseDecimal(parts[2]);

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][SetBreakpoint] Setting breakpoint: type=", static_cast<int>(type), ", address=", address, ", length=", length);

    // Only support type 0 (software breakpoint)
    if (type) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][SetBreakpoint] Unsupported breakpoint type: ", static_cast<int>(type));
        sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    VirtualAddress::Type addressType = address.type();
    if (addressType != VirtualAddress::Type::Module) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] Breakpoint must be in module code region, got type: ", (int)addressType);
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    if (m_breakpointManager->findBreakpoint(address)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] Breakpoint already exists at address: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    uint8_t* pc = address.toPhysicalPC(m_moduleManager);
    if (!pc) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] Failed to convert virtual address to physical: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    m_breakpointManager->setBreakpoint(address, Breakpoint(pc, Breakpoint::Type::Regular));
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][SetBreakpoint] Successfully set breakpoint at ", address, " (physical: ", RawPointer(pc), ", original: 0x", hex(*pc, 2, Lowercase), ")");
    sendReplyOK();
}

void ExecutionHandler::removeBreakpoint(StringView packet)
{
    if (packet.isEmpty())
        return;

    // Format: z0,<address>,<length>
    if (packet.isEmpty() || packet[0] != 'z') {
        sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    StringView params = packet.substring(1);
    auto parts = splitWithDelimiters(params, ",,"_s);
    if (parts.size() != 3) {
        sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    uint32_t type = parseDecimal(parts[0]);
    VirtualAddress address = VirtualAddress(parseHex(parts[1]));

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Removing breakpoint: type=", static_cast<int>(type), ", address=", address);

    // Only support type 0 (software breakpoint)
    if (type) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Unsupported breakpoint type for removal: ", static_cast<int>(type));
        sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    // Delegate to breakpoint manager
    if (m_breakpointManager->removeBreakpoint(address)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Breakpoint removed successfully from ", address);
        sendReplyOK();
    } else {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Breakpoint not found at address: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
    }
}

void ExecutionHandler::handleThreadStopInfo(StringView packet)
{
    // Format: qThreadStopInfo<thread-id-in-hex>
    // Parse the thread ID
    StringView threadIdStr = packet.substring(strlen("qThreadStopInfo"));
    uint64_t threadId = parseHex(threadIdStr);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling qThreadStopInfo for thread: ", threadId);

    Locker locker { m_lock };
    sendStopReplyForThread(locker, threadId);
}

static uint64_t getStopPC(const DebugState& state)
{
    if (state.atBreakpoint() || state.atPrologue())
        return state.stopData->address;
    return VirtualAddress(VirtualAddress::INVALID_BASE).value();
}

static String getThreadName(const DebugState& state, uint64_t threadId)
{
    StringView stateName;
    if (state.atBreakpoint())
        stateName = "wasm-call"_s;
    else if (state.atPrologue())
        stateName = "wasm-prologue"_s;
    else {
        RELEASE_ASSERT(state.atSystemCall());
        stateName = "system-call"_s;
    }
    return makeString(stateName, " tid:0x"_s, hex(threadId, Lowercase));
}

struct ThreadInfo {
    uint64_t threadId;
    uint64_t pc;
    String name;
    StringView stopReason;
};

static Vector<ThreadInfo> collectAllStoppedThreads()
{
    Vector<ThreadInfo> threads;
    VMManager::forEachVM([&](VM& vm) {
        auto* state = vm.debugState();
        RELEASE_ASSERT(state->isStopped());
        uint64_t threadId = ExecutionHandler::threadId(vm);

        StopData::Code code = state->atSystemCall() ? StopData::Code::Stop : state->stopData->code;
        auto stopInfo = stopReasonCodeToInfo(code);
        threads.append({ threadId, getStopPC(*state), getThreadName(*state, threadId), stopInfo.reasonSuffix });
        return IterationStatus::Continue;
    });
    return threads;
}

void ExecutionHandler::sendStopReply(AbstractLocker& locker) WTF_REQUIRES_LOCK(m_lock)
{
    sendStopReplyForThread(locker, threadId(*m_debuggee));
}

void ExecutionHandler::sendStopReplyForThread(AbstractLocker& locker, uint64_t threadId) WTF_REQUIRES_LOCK(m_lock)
{
    VM* vm = findVM(threadId);
    DebugState* state = vm->debugState();
    if (!vm || !state) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] sendStopReplyForThread: thread ", threadId, " not found");
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    RELEASE_ASSERT(state->isStopped());

    // Gather information for the target thread
    Vector<ThreadInfo> allThreads = collectAllStoppedThreads();

    // FIXME: Report different stop reasons for active vs passive threads (currently all use same code).
    StopData::Code code = state->atBreakpoint() ? state->stopData->code : StopData::Code::Stop;
    auto stopInfo = stopReasonCodeToInfo(code);

    // Build packet with target thread
    StringBuilder reply;
    reply.append(stopInfo.reasonString);
    reply.append("thread:"_s, hex(threadId, Lowercase), ';');
    reply.append("name:"_s, getThreadName(*state, threadId), ';');

    // All thread IDs
    reply.append("threads:"_s);
    for (size_t i = 0; i < allThreads.size(); ++i) {
        if (i > 0)
            reply.append(',');
        reply.append(hex(allThreads[i].threadId, Lowercase));
    }
    reply.append(';');

    // All thread PCs
    reply.append("thread-pcs:"_s);
    for (size_t i = 0; i < allThreads.size(); ++i) {
        if (i > 0)
            reply.append(',');
        reply.append(hex(allThreads[i].pc, 16, Lowercase));
    }
    reply.append(';');

    reply.append("00:"_s, toNativeEndianHex(getStopPC(*state)), ';');
    reply.append("reason:"_s, stopInfo.reasonSuffix, ';');

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sending stop reply: target thread="_s, hex(threadId), ", total threads="_s, allThreads.size(), ", packet="_s, reply.toString());
    sendReplyImpl(locker, reply.toString());
}

void ExecutionHandler::sendReply(StringView reply)
{
    Locker locker { m_lock };
    sendReplyImpl(locker, reply);
}

void ExecutionHandler::sendReplyImpl(AbstractLocker&, StringView reply) WTF_REQUIRES_LOCK(m_lock)
{
    uint8_t checksum = 0;
    for (auto character : reply.codeUnits())
        checksum += character;

    String packet = makeString('$', reply, '#', hex(checksum, 2, Lowercase));

#if ENABLE(REMOTE_INSPECTOR)
    if (m_debugServer.isRWIMode()) {
        RELEASE_ASSERT(!!m_debugServer.m_rwiResponseHandler);
        if (m_debugServer.m_rwiResponseHandler(packet)) {
            m_debuggerState = DebuggerState::Replied;
            dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sent reply via RWI: ", packet);
        } else
            dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to send packet via RWI: ", packet);
        return;
    }
#endif

    CString packetData = packet.utf8();
    int sent = static_cast<int>(send(m_debugServer.m_clientSocket, packetData.data(), packetData.length(), 0));
    if (sent < 0)
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to send packet: ", packetData.data(), " sent: ", sent);
    else {
        m_debuggerState = DebuggerState::Replied;
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sent reply: ", packetData.data());
    }
}

void ExecutionHandler::reset()
{
    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling client disconnection in ExecutionHandler");

    if (m_debuggee && debuggeeState()->isStopped())
        resumeImpl(locker);

    m_breakpointManager->clearAllBreakpoints();
    m_debuggerState = DebuggerState::Replied;
    takeAwaitingResumeNotification();
    m_debuggee = nullptr;
}

void ExecutionHandler::sendReplyOK() { m_debugServer.sendReplyOK(); }
void ExecutionHandler::sendErrorReply(ProtocolError error) { m_debugServer.sendErrorReply(error); }

uint64_t ExecutionHandler::threadId(const VM& vm)
{
    auto uid = vm.ownerThreadUID();
    RELEASE_ASSERT(uid.has_value());
    return *uid;
}

DebugState* ExecutionHandler::debuggeeState() const { return m_debuggee->debugState(); }

DebugState* ExecutionHandler::debuggeeStateSafe() const
{
    Locker locker { m_lock };
    RELEASE_ASSERT(m_debuggee);
    return m_debuggee->debugState();
}

bool ExecutionHandler::hasBreakpoints() const
{
    return m_breakpointManager && m_breakpointManager->hasBreakpoints();
}

String ExecutionHandler::callStackStringFor(uint64_t threadId)
{
    Locker locker { m_lock };

    VM* targetVM = m_debuggee;
    if (this->threadId(*targetVM) != threadId)
        targetVM = findVM(threadId);

    if (!targetVM) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] callStackStringFor: thread ", threadId, " not found");
        return String();
    }

    auto* state = targetVM->debugState();
    RELEASE_ASSERT(state->isStopped());

    // For threads stopped at breakpoint with full call stack, walk the stack
    if (state->atBreakpoint()) {
        auto& stopData = *state->stopData;
        RELEASE_ASSERT(stopData.callFrame);

        Vector<VirtualAddress> frameAddresses;
        frameAddresses.append(stopData.address);
        CallFrame* currentFrame = stopData.callFrame;
        uint8_t* returnPC = nullptr;
        VirtualAddress virtualReturnPC;
        unsigned frameIndex = 0;

        // FIXME: Only supports consecutive wasm->wasm calls. Need to support interleaved wasm<->js calls.
        while (getWasmReturnPC(currentFrame, returnPC, virtualReturnPC) && frameIndex < 100) {
            frameAddresses.append(virtualReturnPC);
            currentFrame = currentFrame->callerFrame();
            frameIndex++;
        }

        StringBuilder result;
        for (VirtualAddress address : frameAddresses)
            result.append(toNativeEndianHex(address));

        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] callStackStringFor: collected ", frameAddresses.size(), " frames");
        return result.toString();
    }

    uint64_t pc = getStopPC(*state);
    return toNativeEndianHex(pc);
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
