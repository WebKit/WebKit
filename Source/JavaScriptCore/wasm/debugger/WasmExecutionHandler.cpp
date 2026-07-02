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
#include "WasmExceptionType.h"
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
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExecutionHandler);

struct StopReasonInfo {
    String reasonString;
    StringView reasonSuffix;
};

// GDB RSP stop reply: T<AA> where AA is the POSIX signal number in two-digit hex.
// "reason:<suffix>" is an LLDB extension (not in the GDB RSP spec).
// Reference: https://sourceware.org/gdb/current/onlinedocs/gdb/Stop-Reply-Packets.html
static String signalStopString(int signo)
{
    return makeString('T', hex(signo, 2, WTF::Uppercase));
}

// Maps DebugState::Reason to GDB RSP T-packet signal and reason suffix.
// Reference: https://sourceware.org/gdb/current/onlinedocs/gdb/Stop-Reply-Packets.html
static inline StopReasonInfo stopReasonToInfo(const DebugState& state)
{
    RELEASE_ASSERT(state.stopReason.has_value());
    switch (*state.stopReason) {
    case DebugState::Reason::Interrupted:
        return { signalStopString(SIGSTOP), "signal"_s };
    case DebugState::Reason::Breakpoint:
        return { signalStopString(SIGTRAP), "breakpoint"_s };
    case DebugState::Reason::Step:
        return { signalStopString(SIGTRAP), "trace"_s };
    case DebugState::Reason::WasmTrap:
        return { signalStopString(SIGTRAP), "exception"_s };
    }
    RELEASE_ASSERT_NOT_REACHED();
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
        case StopTheWorldEvent::WasmStepIntoSiteReached:
            RELEASE_ASSERT(m_debuggee == info.targetVM && info.worldMode == VMManager::Mode::RunOne);
            break;
        case StopTheWorldEvent::WasmProgramStop:
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

DebuggerTrapStatus ExecutionHandler::handleDebuggerTrapIfNeeded(CallFrame* callFrame, JSWebAssemblyInstance* instance, IPIntCallee* callee, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack, Wasm::ExceptionType exceptionType)
{
    VM& debuggee = instance->vm();
    if (exceptionType == Wasm::ExceptionType::Unreachable && hasBreakpoints()) {
        VirtualAddress address = VirtualAddress::toVirtual(instance, callee->functionIndex(), pc);
        if (auto* breakpoint = m_breakpointManager->findBreakpoint(address)) {
            debuggee.debugState()->setBreakpointStopData(breakpoint->type, address, breakpoint->originalBytecode, pc, mc, stack, callee, instance, callFrame);
            dataLogLnIf(Options::verboseWasmDebugger(), "[Code][handleDebuggerTrapIfNeeded] Breakpoint at ", *breakpoint, " with ", *debuggee.debugState()->stopData);
            stopTheWorld(debuggee, StopTheWorldEvent::WasmProgramStop);
            return DebuggerTrapStatus::ResolvedByDebugger; // Don't throw; resume execution at this breakpoint
        }
    }

    if (!m_debugServer.isDebuggerReady())
        return DebuggerTrapStatus::NotResolvedByDebugger; // Throw; no debugger connected

    if (exceptionType == Wasm::ExceptionType::StackOverflow || exceptionType == Wasm::ExceptionType::Termination) {
        // Prologue trap: pc/mc/stack are caller's, not the overflowing function's.
        // handleTrapsIfNeeded() may have already processed a NeedStopTheWorld trap,
        // serving a debugger stop and clearing stopData via clearStop(); re-establish
        // prologue context if needed.
        if (!debuggee.debugState()->stopData)
            debuggee.debugState()->setPrologueStopData(instance, callee, callFrame);
        debuggee.debugState()->stopReason = DebugState::Reason::WasmTrap;
        debuggee.debugState()->stopData->wasmTrapType = exceptionType;
    } else
        debuggee.debugState()->setTrapStopData(callee, instance, callFrame, pc, mc, stack, exceptionType);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][handleDebuggerTrapIfNeeded] Wasm trap at ", *debuggee.debugState()->stopData);
    stopTheWorld(debuggee, StopTheWorldEvent::WasmProgramStop);
    return DebuggerTrapStatus::NotResolvedByDebugger; // Throw; trap was reported, now propagate it
}

ExecutionHandler::ResumeMode ExecutionHandler::stopCode(Locker<Lock>& locker, StopTheWorldEvent event)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Start with event:", event);

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
    case StopTheWorldEvent::WasmAtomicsWaitBlocked:
        RELEASE_ASSERT(m_debuggerState == DebuggerState::InterruptRequested || m_debuggerState == DebuggerState::SwitchRequested);
        m_breakpointManager->clearAllOneTimeBreakpoints();
        notifyDebuggerOfStop();
        break;
    case StopTheWorldEvent::WasmProgramStop:
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested || m_debuggerState == DebuggerState::ContinueRequested || m_debuggerState == DebuggerState::SwitchRequested);
        // FIXME: For module-load stops (isNewModuleLoad), step breakpoints should be preserved
        // so the in-progress step can complete after LLDB resumes. Clearing them here silently
        // cancels any active step. This also affects future LLDB expression evaluation, which can
        // trigger module loads internally and should not interrupt a step.
        m_breakpointManager->clearAllOneTimeBreakpoints();
        notifyDebuggerOfStop();
        break;
    case StopTheWorldEvent::WasmStepIntoSiteReached:
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

    // Stop the code and let debugger decide resume mode.
    ExecutionHandler::ResumeMode mode = stopCode(locker, event);
    switch (mode) {
    case ExecutionHandler::ResumeMode::One:
        return STW_RESUME_ONE(m_debuggee);
    case ExecutionHandler::ResumeMode::All:
        m_debuggee = nullptr;
        return STW_RESUME_ALL();
    case ExecutionHandler::ResumeMode::Switch:
        RELEASE_ASSERT(m_debuggee != &debuggee);
        RELEASE_ASSERT(debuggee.debugState()->isStopped);
        RELEASE_ASSERT(m_debuggee->debugState()->isStopped);
        return STW_CONTEXT_SWITCH(m_debuggee);
    }
    RELEASE_ASSERT_NOT_REACHED();
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
        if (vm.debugState()->isStopped && debugState->isStoppedAtPrologue()) {
            selectedVM = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });

    m_debuggee = selectedVM ? selectedVM : &fallbackVM;
    dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Selected debuggee VM:", RawPointer(m_debuggee));
}

// Called by VMManager when ALL VMs are stopped at safe points.
// Returns resume mode (ResumeOne/ResumeAll/ContextSwitch) to coordinate execution.
StopTheWorldStatus wasmDebuggerOnStopCallback(VM& debuggee, StopTheWorldEvent event)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[STW] Callback invoked with event:", event);
    auto& server = DebugServer::singleton();
    if (!server.hasDebugger()) {
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
    if (!server.hasDebugger()) {
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

    RELEASE_ASSERT(debuggeeState()->isStopped);
    m_debuggerState = DebuggerState::ContinueRequested;
    m_debuggeeContinue.notifyOne(); // Notify debuggee VM with resume all command.

    // Wait for resume to complete to prevent interrupt() from acquiring lock before VMs resume.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Notified code to continue and waiting...");
    m_debuggerContinue.wait(locker); // Wait for resume to complete.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Confirmed that code is running...");
}

void ExecutionHandler::notifyDebuggerOfNewModule(VM& vm)
{
    vm.debugState()->isNewModuleLoad = true;
    stopTheWorld(vm, StopTheWorldEvent::WasmProgramStop);
}

static inline VM* findVM(uint64_t vmId)
{
    VM* result = nullptr;
    VMManager::forEachVM([&](VM& vm) {
        if (vm.debugState()->isStopped && vmId == vm.identifier().toRawValue()) {
            result = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return result;
}

void ExecutionHandler::switchTarget(uint64_t vmId)
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == debugServerThreadId());

    Locker locker { m_lock };

    VM* newDebuggee = findVM(vmId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][SwitchVM] current debuggee=", RawPointer(m_debuggee), " new debuggee=", RawPointer(newDebuggee));

    if (m_debuggee == newDebuggee)
        return;

    RELEASE_ASSERT(debuggeeState()->isStopped);
    m_debuggee = newDebuggee;
    m_debuggerState = DebuggerState::SwitchRequested;
    m_debuggeeContinue.notifyOne(); // Notify to switch VM context.

    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][SwitchVM] Notified code to continue and switch VM, waiting...");
    m_debuggerContinue.wait(locker); // Wait for new debuggee VM to stop.
    RELEASE_ASSERT(debuggeeState()->isStopped);
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

    {
        RELEASE_ASSERT(!m_debuggee || !debuggeeState()->isStopped);
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
    RELEASE_ASSERT(m_debuggerState == DebuggerState::Replied && state->isStopped);

    bool resumeAll = false;
    if (state->isStoppedAtSystemCall() || state->isStoppedDueToWasmTrap()) {
        // There is no valid next WASM instruction to step to in either case.
        // For traps (including StackOverflow at prologue), execution unwinds back to JS —
        // resuming all is the right behavior.
        resumeAll = true;
    } else if (state->isStoppedAtBytecode())
        resumeAll = stepAtBytecode(locker, state);
    else {
        RELEASE_ASSERT(state->isStoppedAtPrologue());
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

bool ExecutionHandler::stepAtBytecode(Locker<Lock>& locker, DebugState* state)
{
    RELEASE_ASSERT(state->isStoppedAtBytecode());
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

    // If no one-time breakpoints registered, or stopped at memory.atomic.wait (must resumeAll
    // so notifier threads can run), then resume all VMs instead of using RunOne.
    return !m_breakpointManager->hasOneTimeBreakpoints() || state->isAtomicsWaitStop();
}

void ExecutionHandler::setStepIntoBreakpointForCall(VM& callerVM, CalleeBits boxedCallee, JSWebAssemblyInstance* calleeInstance)
{
    if (!callerVM.debugState()->takeStepIntoCall())
        return;

    [&]() {
        Locker locker { m_lock };

        RELEASE_ASSERT(m_debuggee == &callerVM);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][StepIntoEvent] Start for call");
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StepRequested);

        RELEASE_ASSERT(calleeInstance);
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

    stopTheWorld(callerVM, StopTheWorldEvent::WasmStepIntoSiteReached);
}

void ExecutionHandler::setStepIntoBreakpointForThrow(VM& throwVM)
{
    if (!throwVM.debugState()->takeStepIntoThrow())
        return;

    [&]() {
        Locker locker { m_lock };

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

    stopTheWorld(throwVM, StopTheWorldEvent::WasmStepIntoSiteReached);
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
    uint64_t vmId = parseHex(threadIdStr);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling qThreadStopInfo for thread: ", vmId);

    Locker locker { m_lock };
    sendStopReplyForThread(locker, vmId);
}

static uint64_t NODELETE getStopPC(const DebugState& state)
{
    if (state.isStoppedAtSystemCall())
        return VirtualAddress(VirtualAddress::INVALID_BASE).value();
    RELEASE_ASSERT(state.stopData);
    return state.stopData->address;
}

static String getThreadName(const DebugState& state, uint64_t vmId)
{
    StringView stateName;
    if (state.isStoppedAtPrologue())
        stateName = "wasm-prologue"_s;
    else if (state.isStoppedAtBytecode())
        stateName = "wasm-call"_s;
    else {
        RELEASE_ASSERT(state.isStoppedAtSystemCall());
        stateName = "system-call"_s;
    }
    return makeString(stateName, " tid:0x"_s, hex(vmId, Lowercase));
}

struct ThreadInfo {
    uint64_t vmId;
    uint64_t pc;
    String name;
    StringView stopReason;
};

void ExecutionHandler::sendStopReply(AbstractLocker& locker) WTF_REQUIRES_LOCK(m_lock)
{
    sendStopReplyForThread(locker, m_debuggee->identifier().toRawValue());
}

void ExecutionHandler::sendStopReplyForThread(AbstractLocker& locker, uint64_t vmId) WTF_REQUIRES_LOCK(m_lock)
{
    VM* vm = findVM(vmId);
    if (!vm) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] sendStopReplyForThread: thread ", vmId, " not found");
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    DebugState* state = vm->debugState();
    if (!state) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] sendStopReplyForThread: thread ", vmId, " not found");
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    RELEASE_ASSERT(state->isStopped);

    // Collect all stopped threads; swap event thread to index 0 so thread-pcs[i] aligns with threads[i].
    Vector<ThreadInfo> allThreads;
    VMManager::forEachVM([&](VM& vm) {
        auto* state = vm.debugState();
        if (!state->isStopped)
            return IterationStatus::Continue;
        uint64_t tid = vm.identifier().toRawValue();
        allThreads.append({ tid, getStopPC(*state), getThreadName(*state, tid), stopReasonToInfo(*state).reasonSuffix });
        if (tid == vmId)
            std::swap(allThreads[0], allThreads.last());
        return IterationStatus::Continue;
    });

    // A "passive thread" is a VM that was collaterally stopped when the world was halted (its
    // stopReason set to Interrupted by setStopped()), as opposed to the debuggee thread that
    // actually triggered the stop event (breakpoint/step/trap/interrupt/new-module-load).
    // Passive threads get signal 0 so LLDB's ShouldSelect() returns false for them, allowing
    // the event thread to win thread selection in HandleProcessStateChangedEvent.
    bool isPassiveThread = state->stopReason == DebugState::Reason::Interrupted && m_debuggee && vmId != m_debuggee->identifier().toRawValue();
    auto stopInfo = isPassiveThread
        ? StopReasonInfo { signalStopString(0), "signal"_s }
        : stopReasonToInfo(*state);

    // Build packet with target thread
    StringBuilder reply;
    reply.append(stopInfo.reasonString);
    reply.append("thread:"_s, hex(vmId, Lowercase), ';');
    reply.append("name:"_s, getThreadName(*state, vmId), ';');

    // All thread IDs
    reply.append("threads:"_s);
    for (size_t i = 0; i < allThreads.size(); ++i) {
        if (i > 0)
            reply.append(',');
        reply.append(hex(allThreads[i].vmId, Lowercase));
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

    // Append library:; to prompt LLDB to re-query qXfer:libraries:read when there are pending
    // library changes: (1) new-module-load stop, (2) piggybacked on any natural stop when a module
    // was loaded but no dedicated stop fired yet, (3) module removal via unregisterModule().
    // Gated on isDebuggerReady() to avoid sending library:; in the ? reply before the initial
    // qXfer:libraries:read handshake completes.
    if (m_moduleManager.needsLibraryRequery() && m_debugServer.isDebuggerReady()) {
        reply.append("library:;"_s);
        // Include a human-readable description only for dedicated new-module-load stops.
        if (state->isNewModuleLoad) {
            RELEASE_ASSERT(state->isStoppedAtSystemCall());
            reply.append("description:"_s);
            StringBuilder description;
            description.append("loaded new wasm module with ids: "_s);
            auto ids = m_moduleManager.unnotifiedModuleIds();
            for (size_t i = 0; i < ids.size(); ++i) {
                if (i)
                    description.append(", "_s);
                description.append(ids[i]);
            }
            for (UChar c : StringView(description.toString()).codeUnits())
                reply.append(hex(static_cast<uint8_t>(c), 2, Lowercase));
            reply.append(';');
        }
    }

    // For trap stops, include a hex-encoded description so LLDB can display the trap reason.
    if (state->isStoppedDueToWasmTrap()) {
        reply.append("description:"_s);
        for (UChar c : StringView(Wasm::errorMessageForExceptionType(*state->stopData->wasmTrapType)).codeUnits())
            reply.append(hex(static_cast<uint8_t>(c), 2, Lowercase));
        reply.append(';');
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sending stop reply: target thread="_s, hex(vmId), ", total threads="_s, allThreads.size(), ", packet="_s, reply.toString());
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

    // Clear before resuming: resumeImpl() transiently releases m_lock, and the
    // VM must not re-hit a breakpoint in that window.
    m_breakpointManager->clearAllBreakpoints();

    if (m_debuggee && debuggeeState()->isStopped)
        resumeImpl(locker);

    m_debuggerState = DebuggerState::Replied;
    takeAwaitingResumeNotification();
    m_debuggee = nullptr;
}

void ExecutionHandler::sendReplyOK() { m_debugServer.sendReplyOK(); }
void ExecutionHandler::sendErrorReply(ProtocolError error) { m_debugServer.sendErrorReply(error); }


DebugState* ExecutionHandler::debuggeeState() const { return m_debuggee->debugState(); }

DebugState* ExecutionHandler::debuggeeStateForTest() const
{
    Locker locker { m_lock };
    RELEASE_ASSERT(m_debuggee);
    return m_debuggee->debugState();
}

bool ExecutionHandler::hasBreakpoints() const
{
    return m_breakpointManager && m_breakpointManager->hasBreakpoints();
}

String ExecutionHandler::callStackStringFor(uint64_t vmId)
{
    Locker locker { m_lock };

    VM* targetVM = m_debuggee;
    RELEASE_ASSERT(targetVM);
    if (targetVM->identifier().toRawValue() != vmId)
        targetVM = findVM(vmId);

    if (!targetVM) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] callStackStringFor: thread ", vmId, " not found");
        return String();
    }

    auto* state = targetVM->debugState();
    RELEASE_ASSERT(state->isStopped);

    if (state->stopData) {
        auto& stopData = *state->stopData;
        RELEASE_ASSERT(stopData.callFrame);

        Vector<FrameInfo> frames = collectCallStack(stopData.address, stopData.callFrame, *targetVM);

        StringBuilder result;
        for (const auto& frame : frames)
            result.append(toNativeEndianHex(frame.address));

        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] callStackStringFor: collected ", frames.size(), " frames");
        return result.toString();
    }

    uint64_t pc = getStopPC(*state);
    return toNativeEndianHex(pc);
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
