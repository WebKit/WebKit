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

#pragma once

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "StopTheWorldCallback.h"
#include "WasmDebugServer.h"
#include "WasmModuleManager.h"
#include "WasmVirtualAddress.h"

#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace JSC {

class CalleeBits;
class CallFrame;
class JSWebAssemblyInstance;
class VM;

namespace IPInt {
struct IPIntLocal;
struct IPIntStackEntry;
}

namespace Wasm {

class IPIntCallee;
class BreakpointManager;

class ExecutionHandler {
    WTF_MAKE_TZONE_ALLOCATED(ExecutionHandler);

public:
    ExecutionHandler(DebugServer&, ModuleManager&);

    enum class ResumeMode : uint8_t {
        One, // Resume only debuggee VM
        All, // Resume all VMs
        Switch, // Switch to current debuggee VM
    };

    enum class DebuggerState : uint8_t {
        Replied, // Sent reply to LLDB, waiting for next command.
        InterruptRequested, // Interrupt all mutators requested.
        ContinueRequested, // Resume all mutators requested.
        StepRequested, // Step debuggee requested.
        SwitchRequested, // Switch to debuggee requested.
    };

    ResumeMode stopCode(Locker<Lock>&, StopTheWorldEvent) WTF_REQUIRES_LOCK(m_lock);
    bool hitBreakpoint(CallFrame*, JSWebAssemblyInstance*, IPIntCallee*, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* = nullptr, IPInt::IPIntStackEntry* = nullptr);

    JS_EXPORT_PRIVATE void resume();
    JS_EXPORT_PRIVATE void step();
    JS_EXPORT_PRIVATE void interrupt();
    void handleThreadStopInfo(StringView packet);
    String callStackStringFor(uint64_t threadId);
    JS_EXPORT_PRIVATE void reset();

    JS_EXPORT_PRIVATE void setBreakpointAtEntry(JSWebAssemblyInstance*, IPIntCallee*, Breakpoint::Type);
    void setBreakpointAtPC(JSWebAssemblyInstance*, FunctionCodeIndex, Breakpoint::Type, const uint8_t* pc);
    void setBreakpoint(StringView packet);
    void removeBreakpoint(StringView packet);
    JS_EXPORT_PRIVATE BreakpointManager* breakpointManager() { return m_breakpointManager.get(); };

    void setStepIntoBreakpointForCall(VM&, CalleeBits, JSWebAssemblyInstance*);
    void setStepIntoBreakpointForThrow(VM&);

    bool hasBreakpoints() const;

    JS_EXPORT_PRIVATE static uint64_t threadId(const VM&);
    uint64_t debugServerThreadId() const
    {
        RELEASE_ASSERT(m_debugServerThreadId.has_value());
        return *m_debugServerThreadId;
    }
    void setDebugServerThreadId(uint64_t threadId) { m_debugServerThreadId = threadId; }

    JS_EXPORT_PRIVATE DebugState* debuggeeState() const WTF_REQUIRES_LOCK(m_lock);
    JS_EXPORT_PRIVATE DebugState* debuggeeStateSafe() const; // FIXME: Should be used for test only

    VM* debuggeeVM() const // Used for test only
    {
        Locker locker { m_lock };
        return m_debuggee;
    }

    void notifyVMDestruction(VM* vm)
    {
        Locker locker { m_lock };
        if (m_debuggee == vm)
            m_debuggee = nullptr;
    }

    JS_EXPORT_PRIVATE void switchTarget(uint64_t threadId);

    StopTheWorldStatus handleStopTheWorld(VM&, StopTheWorldEvent);
    void handlePostResume();
    bool takeAwaitingResumeNotification() WTF_REQUIRES_LOCK(m_lock) { return std::exchange(m_awaitingResumeNotification, false); }

private:
    friend class DebugServer;

    void stopTheWorld(VM&, StopTheWorldEvent);

    ResumeMode stopImpl(Locker<Lock>&) WTF_REQUIRES_LOCK(m_lock);

    void resumeImpl(Locker<Lock>&) WTF_REQUIRES_LOCK(m_lock);

    bool stepAtBreakpoint(Locker<Lock>&, DebugState*) WTF_REQUIRES_LOCK(m_lock);

    void sendStopReply(AbstractLocker&);
    void sendStopReplyForThread(AbstractLocker&, uint64_t threadId);
    void sendReplyOK();
    void sendReply(StringView reply);
    void sendReplyImpl(AbstractLocker&, StringView reply);
    void sendErrorReply(ProtocolError);

    void selectDebuggeeIfNeeded(VM& fallbackVM) WTF_REQUIRES_LOCK(m_lock);
    void markVMStates(VM* debuggee) WTF_REQUIRES_LOCK(m_lock);
    void clearOtherVMStopData(VM* debuggee) WTF_REQUIRES_LOCK(m_lock);

    bool requiresStopConfirmation() const WTF_REQUIRES_LOCK(m_lock)
    {
        switch (m_debuggerState) {
        case DebuggerState::InterruptRequested:
        case DebuggerState::StepRequested:
        case DebuggerState::SwitchRequested:
            return true;
        default:
            return false;
        }
    }

    DebugServer& m_debugServer;
    ModuleManager& m_moduleManager;
    std::unique_ptr<BreakpointManager> m_breakpointManager;

    mutable Lock m_lock;
    Condition m_debuggerContinue;
    Condition m_debuggeeContinue;
    DebuggerState m_debuggerState WTF_GUARDED_BY_LOCK(m_lock) { DebuggerState::Replied };
    bool m_awaitingResumeNotification WTF_GUARDED_BY_LOCK(m_lock) { false };
    VM* m_debuggee WTF_GUARDED_BY_LOCK(m_lock) { nullptr };
    std::optional<uint64_t> m_debugServerThreadId;
};

StopTheWorldStatus wasmDebuggerOnStopCallback(VM&, StopTheWorldEvent);
void wasmDebuggerOnResumeCallback();

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
