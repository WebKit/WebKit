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

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "Options.h"
#include "SubspaceInlines.h"
#include "VM.h"
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
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExecutionHandler);

// FIXME: This current implementation only stops a single VM. In real-world browser debugging,
// when ANY VM hits a WASM breakpoint, we should stop ALL execution across ALL VMs in the process.
//
// COMPREHENSIVE STOP-THE-WORLD APPROACH:
// - Single VM with WASM: Current implementation works (but should be consistent)
// - TODO: Multiple VMs, only one running WASM: Stop ALL VMs (WASM + non-WASM mutators)
// - TODO: Multiple VMs, multiple running WASM: Stop ALL VMs (all WASM + all non-WASM mutators)
class StopWorld {
public:
    explicit StopWorld(VM& vm)
        : m_vm(vm)
    {
    }

    ~StopWorld()
    {
        if (m_vm.isWasmStopWorldActive())
            deactivateStopWorld();
    }

    void activateStopWorld()
    {
        m_vm.traps().requestStop();
        m_vm.setIsWasmStopWorldActive(true);
    }

    void deactivateStopWorld()
    {
        m_vm.traps().cancelStop();
        m_vm.setIsWasmStopWorldActive(false);
    }

private:
    VM& m_vm;
};

struct StopReasonInfo {
    String reasonString;
    StringView reasonSuffix;
};

static inline StopReasonInfo stopReasonCodeToInfo(ExecutionHandler::StopReason::Code code)
{
    switch (code) {
    case ExecutionHandler::StopReason::Code::Signal:
        return { "T02"_s, "signal"_s }; // SIGINT - Interrupt signal
    case ExecutionHandler::StopReason::Code::Trace:
        return { "T05"_s, "trace"_s }; // SIGTRAP - Trace/single step
    case ExecutionHandler::StopReason::Code::Breakpoint:
        return { "T05"_s, "breakpoint"_s }; // SIGTRAP - Breakpoint hit
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { String(), "trace"_s };
    }
}

ExecutionHandler::ExecutionHandler(DebugServer& debugServer, ModuleManager& instanceManager, BreakpointManager& breakpointManager)
    : m_debugServer(debugServer)
    , m_instanceManager(instanceManager)
    , m_breakpointManager(breakpointManager)
{
}

template<typename LockType>
void ExecutionHandler::stopImpl(LockType& locker) WTF_REQUIRES_LOCK(m_lock)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop][Breakpoint] Updated stop reason and waiting...");
    m_mutatorContinue.wait(locker);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop][Breakpoint] Unblocked and running...");

    m_stopReason.reset();
    m_mutatorState = MutatorState::Running;
    if (m_debuggerState == DebuggerState::ContinueRequested)
        m_debuggerContinue.notifyOne();
}

void ExecutionHandler::stopOneTimeBreakpoint(StopReason&& stopReason)
{
    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop][OneTimeBreakpoint] Start");

    m_stopReason = stopReason;
    m_mutatorState = MutatorState::Stopped;
    m_breakpointManager.clearAllTmpBreakpoints();

    RELEASE_ASSERT(m_debuggerState == DebuggerState::StopRequested);
    m_debuggerContinue.notifyOne();

    stopImpl(locker);
}

void ExecutionHandler::stopRegularBreakpoint(StopReason&& stopReason)
{
    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop][RegularBreakpoint] Start");

    m_stopReason = stopReason;
    m_mutatorState = MutatorState::Stopped;
    if (m_debuggerState == DebuggerState::ContinueRequested) {
        sendStopReply(locker);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop][RegularBreakpoint] Currently in continue. Sent a stop reply and waiting...");
    } else {
        RELEASE_ASSERT(m_debuggerState == DebuggerState::StopRequested);
        m_debuggerContinue.notifyOne();
    }

    stopImpl(locker);
}

bool ExecutionHandler::stopCode(CallFrame* callFrame, JSWebAssemblyInstance* instance, IPIntCallee* callee, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* locals, IPInt::IPIntStackEntry* stack)
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == m_debugServer.mutatorThreadId());

    VirtualAddress address = VirtualAddress::toVirtual(instance, callee->functionIndex(), pc);
    if (auto* breakpoint = m_breakpointManager.findBreakpoint(address)) {
        StopReason stopReason(breakpoint->type, address, breakpoint->originalBytecode, pc, mc, locals, stack, callee, instance, callFrame);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Code][Stop] Going to stop at ", *breakpoint, " with ", stopReason);
        if (breakpoint->isOneTimeBreakpoint())
            stopOneTimeBreakpoint(WTFMove(stopReason));
        else
            stopRegularBreakpoint(WTFMove(stopReason));
        return true;
    }
    return false;
}

void ExecutionHandler::resume()
{
    Locker locker { m_lock };

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Start");

    RELEASE_ASSERT(Thread::currentSingleton().uid() == m_debugServer.debugServerThreadId());
    RELEASE_ASSERT(m_debuggerState == DebuggerState::Replied && m_mutatorState == MutatorState::Stopped);
    m_mutatorContinue.notifyOne();

    // This is to simplify implementation. If we don't wait here, we may have a race condition that
    // after above notification, interrupt() may acquire the locker first.
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Notified code to continue and waiting...");
    m_debuggerState = DebuggerState::ContinueRequested;
    m_debuggerContinue.wait(m_lock);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Continue] Confirmed that code is running...");
}

void ExecutionHandler::interrupt()
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == m_debugServer.debugServerThreadId());

    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Start");

    // LLDB implements interrupt flood prevention: once LLDB sends the first interrupt packet,
    // no matter how many Ctrl+C the user types, LLDB will not send additional interrupt packets
    // until it receives a stop reply. This prevents packet flooding and ensures clean protocol behavior.
    // Our WebKit implementation handles each interrupt request by activating StopWorld via VM traps.
    StopWorld stopWorld(*m_debugServer.vm());

    {
        RELEASE_ASSERT(m_mutatorState == MutatorState::Running);
        m_debuggerState = DebuggerState::StopRequested;
        stopWorld.activateStopWorld();
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Notified code to stop and waiting...");
    m_debuggerContinue.wait(m_lock);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Interrupt] Confirmed that code is stoped");

    {
        stopWorld.deactivateStopWorld();
        sendStopReply(locker);
    }
}

void ExecutionHandler::step()
{
    RELEASE_ASSERT(Thread::currentSingleton().uid() == m_debugServer.debugServerThreadId());

    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Start with ", m_stopReason);

    uint8_t* currentPC = m_stopReason.pc;
    uint8_t* currentMC = m_stopReason.mc;

    auto setStepTmpBreakpoint = [&](const uint8_t* nextPC) WTF_REQUIRES_LOCK(m_lock) {
        VirtualAddress nextAddress = VirtualAddress(m_stopReason.address.value() + (nextPC - currentPC));
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step][SetTmpBreakpoint] current PC=", RawPointer(currentPC), "(", m_stopReason.address, "), next PC=", RawPointer(nextPC), "(", nextAddress, ")");
        if (m_breakpointManager.findBreakpoint(nextAddress))
            return;
        m_breakpointManager.setBreakpoint(nextAddress, Breakpoint(const_cast<uint8_t*>(nextPC), Breakpoint::Type::Step));
    };

    auto setStepTmpBreakpointAtCaller = [&]() WTF_REQUIRES_LOCK(m_lock) {
        uint8_t* returnPC = nullptr;
        VirtualAddress virtualReturnPC;
        if (getWasmReturnPC(m_stopReason.callFrame, returnPC, virtualReturnPC)) {
            m_breakpointManager.setBreakpoint(virtualReturnPC, Breakpoint(const_cast<uint8_t*>(returnPC), Breakpoint::Type::Step));
            return true;
        }
        return false;
    };

    auto setStepIntoBreakpointForDirectCall = [&]() WTF_REQUIRES_LOCK(m_lock) -> bool {
        const IPInt::CallMetadata* metadata = reinterpret_cast<const IPInt::CallMetadata*>(currentMC);
        Wasm::FunctionSpaceIndex functionSpaceIndex = metadata->functionIndex;

        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Call instruction metadata: function index ", static_cast<uint32_t>(functionSpaceIndex));

        JSWebAssemblyInstance* instance = m_stopReason.instance;
        RefPtr calleeGroup = instance->calleeGroup();
        RefPtr<Wasm::IPIntCallee> callee = calleeGroup->wasmCalleeFromFunctionIndexSpace(functionSpaceIndex);
        if (callee->compilationMode() != Wasm::CompilationMode::IPIntMode) {
            dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Target function is not IPInt mode");
            return false;
        }

        const uint8_t* functionStart = callee->bytecode();
        VirtualAddress address = VirtualAddress::toVirtual(instance, callee->functionIndex(), callee->bytecode());
        m_breakpointManager.setBreakpoint(address, Breakpoint(const_cast<uint8_t*>(functionStart), Breakpoint::Type::Step));
        return true;
    };

    bool needToWaitForStop = true;
    switch (m_stopReason.originalBytecode) {
    case Return:
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Handling return instruction - setting breakpoint at caller");
        needToWaitForStop = setStepTmpBreakpointAtCaller();
        break;
    case Call:
    case TailCall:
        if (setStepIntoBreakpointForDirectCall())
            break;
        [[fallthrough]];
    // FIXME: Need to set step into breakpoints for these calls
    case CallIndirect:
    case TailCallIndirect:
    case CallRef:
    case TailCallRef:
        [[fallthrough]];
    default: {
        const auto& moduleInfo = m_stopReason.instance->moduleInformation();
        auto functionIndex = m_stopReason.callee->functionIndex();
        uint32_t offset = m_stopReason.address.offset();
        const auto* nextInstructions = moduleInfo.debugInfo->ensureFunctionDebugInfo(functionIndex).findNextInstructions(offset);
        RELEASE_ASSERT(nextInstructions, "Didn't find nextInstructions");
        uint8_t* const basePC = m_stopReason.pc - offset;
        for (uint32_t nextOffset : *nextInstructions)
            setStepTmpBreakpoint(basePC + nextOffset);
        break;
    }
    }

    RELEASE_ASSERT(m_debuggerState == DebuggerState::Replied && m_mutatorState == MutatorState::Stopped);
    m_mutatorContinue.notifyOne();

    if (needToWaitForStop) {
        m_debuggerState = DebuggerState::StopRequested;
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Notified code to continue and waiting...");
        m_debuggerContinue.wait(m_lock);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] JSC is stoped");
        sendStopReply(locker);
    } else {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Notified code to continue and waiting...");
        m_debuggerState = DebuggerState::ContinueRequested;
        m_debuggerContinue.wait(m_lock);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger][Step] Confirmed that code is running...");
    }
}

void ExecutionHandler::setInterruptBreakpoint(JSWebAssemblyInstance* instance, IPIntCallee* callee)
{
    uint8_t* pc = const_cast<uint8_t*>(callee->bytecode());
    VirtualAddress address = VirtualAddress::toVirtual(instance, callee->functionIndex(), pc);
    if (m_breakpointManager.findBreakpoint(address))
        return;
    m_breakpointManager.setBreakpoint(address, Breakpoint(pc, Breakpoint::Type::Interrupt));
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

    // Only support software breakpoints for now
    // 0 = Software breakpoint
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

    if (m_breakpointManager.findBreakpoint(address)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] Breakpoint already exists at address: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    uint8_t* pc = address.toPhysicalPC(m_instanceManager);
    if (!pc) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ExecutionHandler] Failed to convert virtual address to physical: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    m_breakpointManager.setBreakpoint(address, Breakpoint(pc, Breakpoint::Type::Regular));
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

    // Only support software breakpoints for now
    // 0 = Software breakpoint
    if (type) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Unsupported breakpoint type for removal: ", static_cast<int>(type));
        sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    // Delegate to breakpoint manager
    if (m_breakpointManager.removeBreakpoint(address)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Breakpoint removed successfully from ", address);
        sendReplyOK();
    } else {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Breakpoint not found at address: ", address);
        sendErrorReply(ProtocolError::InvalidAddress);
    }
}

void ExecutionHandler::handleThreadStopInfo(StringView packet)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling qThreadStopInfo: ", packet);
    Locker locker { m_lock };
    sendStopReply(locker);
}

void ExecutionHandler::sendStopReply(AbstractLocker& locker) WTF_REQUIRES_LOCK(m_lock)
{
    RELEASE_ASSERT(m_mutatorState == MutatorState::Stopped && m_stopReason.isValid());
    uint64_t pc = m_stopReason.address;

    auto stopInfo = stopReasonCodeToInfo(m_stopReason.code);
    String reasonString = stopInfo.reasonString;
    StringView reasonSuffix = stopInfo.reasonSuffix;
    String pcBytes = toNativeEndianHex(pc);
    uint64_t mutatorThreadId = m_debugServer.mutatorThreadId();

    String stopReplyStr = makeString(
        reasonString,
        "thread:"_s, hex(mutatorThreadId, Lowercase),
        ";name:JSC-mutator;threads:"_s, hex(mutatorThreadId, Lowercase),
        ";thread-pcs:"_s, hex(pc, 16, Lowercase),
        ";00:"_s,
        pcBytes,
        ";reason:"_s,
        reasonSuffix,
        ";"_s);

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sending stop reply: ", stopReplyStr);
    sendReply(locker, stopReplyStr);
}

void ExecutionHandler::sendReply(StringView reply)
{
    Locker locker { m_lock };
    sendReply(locker, reply);
}

void ExecutionHandler::sendReply(AbstractLocker&, StringView reply) WTF_REQUIRES_LOCK(m_lock)
{
    uint8_t checksum = 0;
    for (auto character : reply.codeUnits())
        checksum += character;

    String packet = makeString('$', reply, '#', hex(checksum, 2, Lowercase));
    CString packetData = packet.utf8();

    int sent = static_cast<int>(send(m_debugServer.m_clientSocket, packetData.data(), packetData.length(), 0));
    if (sent < 0)
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to send packet: ", packetData.data(), " sent: ", sent);
    else {
        m_debuggerState = ExecutionHandler::DebuggerState::Replied;
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sent reply: ", packetData.data());
    }
}

void ExecutionHandler::reset()
{
    Locker locker { m_lock };
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling client disconnection in ExecutionHandler");

    m_debuggerState = DebuggerState::Replied;

    if (m_mutatorState == MutatorState::Stopped) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Resuming stopped WebAssembly execution due to client disconnection");
        m_mutatorState = MutatorState::Running;
        m_stopReason.reset();
        m_mutatorContinue.notifyAll();
    }
}

void ExecutionHandler::sendReplyOK() { m_debugServer.sendReplyOK(); }
void ExecutionHandler::sendErrorReply(ProtocolError error) { m_debugServer.sendErrorReply(error); }
ExecutionHandler::StopReason ExecutionHandler::stopReason() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    RELEASE_ASSERT(m_stopReason.isValid());
    return m_stopReason;
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
