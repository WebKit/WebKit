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

#if ENABLE(WEBASSEMBLY)

#include "WasmDebugServerUtilities.h"
#include "WasmVirtualAddress.h"

#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace JSC {

class CallFrame;
class JSWebAssemblyInstance;

namespace IPInt {
struct IPIntLocal;
struct IPIntStackEntry;
}

namespace Wasm {

class DebugServer;
class IPIntCallee;
class ModuleManager;
class BreakpointManager;

class ExecutionHandler {
    WTF_MAKE_TZONE_ALLOCATED(ExecutionHandler);

public:
    ExecutionHandler(DebugServer&, ModuleManager&, BreakpointManager&);

    bool stopCode(CallFrame*, JSWebAssemblyInstance*, IPIntCallee*, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* = nullptr, IPInt::IPIntStackEntry* = nullptr);

    void resume();
    void step();
    void interrupt();
    void handleThreadStopInfo(StringView packet);
    void reset();

    void setInterruptBreakpoint(JSWebAssemblyInstance*, IPIntCallee*);
    void setBreakpoint(StringView packet);
    void removeBreakpoint(StringView packet);

    struct StopReason {
        // GDB Remote Protocol stop reason codes mapped to GDB Remote Protocol semantics
        // Reference: https://sourceware.org/gdb/onlinedocs/gdb/Stop-Reply-Packets.html
        enum class Code : uint8_t {
            Unknown = 0,
            Signal, // SIGINT - Interrupt signal (Ctrl+C) - reason:signal
            Trace, // SIGTRAP - Single step/trace completion - reason:trace
            Breakpoint // Custom - Breakpoint hit - reason:breakpoint (distinct from trace)
        };

        StopReason() = default;
        StopReason(Breakpoint::Type type, VirtualAddress address, uint8_t originalBytecode, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* locals, IPInt::IPIntStackEntry* stack, IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame)
            : address(address)
            , originalBytecode(originalBytecode)
            , pc(pc)
            , mc(mc)
            , locals(locals)
            , stack(stack)
            , callee(callee)
            , instance(instance)
            , callFrame(callFrame)
        {
            setCode(type);
        }

        bool isValid() const { return code != Code::Unknown; }

        void setCode(Breakpoint::Type type)
        {
            switch (type) {
            case Breakpoint::Type::Interrupt:
                code = Code::Signal;
                break;
            case Breakpoint::Type::Step:
                code = Code::Trace;
                break;
            case Breakpoint::Type::Regular:
                code = Code::Breakpoint;
                break;
            default:
                break;
            }
        }

        void reset()
        {
            code = Code::Unknown;
            address = VirtualAddress();
            originalBytecode = 0;
            pc = nullptr;
            mc = nullptr;
            locals = nullptr;
            stack = nullptr;
            callee = nullptr;
            instance = nullptr;
            callFrame = nullptr;
        }

        void dump(PrintStream& out) const
        {
            out.print("StopReason(Code:", code);
            out.print(", address:", address);
            out.print(", originalBytecode:", originalBytecode);
            out.print(", pc:", RawPointer(pc));
            out.print(", mc:", RawPointer(mc));
            out.print(", locals:", RawPointer(locals));
            out.print(", stack:", RawPointer(stack));
            out.print(", callee:", RawPointer(callee.get()));
            out.print(", instance:", RawPointer(instance));
            out.print(", callFrame:", RawPointer(callFrame), ")");
        }

        Code code { Code::Unknown };
        VirtualAddress address { VirtualAddress() };
        uint8_t originalBytecode { 0 };
        uint8_t* pc { nullptr };
        uint8_t* mc { nullptr };
        IPInt::IPIntLocal* locals { nullptr };
        IPInt::IPIntStackEntry* stack { nullptr };
        RefPtr<IPIntCallee> callee;
        JSWebAssemblyInstance* instance { nullptr };
        CallFrame* callFrame { nullptr };
    };

    StopReason stopReason() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS;

private:
    friend class DebugServer;

    enum class DebuggerState : uint8_t {
        ReplyFailed,
        Replied,
        StopRequested,
        ContinueRequested,
    };

    enum class MutatorState : uint8_t {
        Running,
        Stopped,
    };

    void stopOneTimeBreakpoint(StopReason&&);
    void stopRegularBreakpoint(StopReason&&);
    template<typename LockType>
    void stopImpl(LockType&);

    void sendStopReply(AbstractLocker&);
    void sendReplyOK();
    void sendReply(StringView reply);
    void sendReplyImpl(AbstractLocker&, StringView reply);
    void sendErrorReply(ProtocolError);
    void handleClientDisconnectionLocked() WTF_REQUIRES_LOCK(m_lock);

    DebugServer& m_debugServer;
    ModuleManager& m_instanceManager;
    BreakpointManager& m_breakpointManager;

    Lock m_lock;
    Condition m_debuggerContinue;
    Condition m_mutatorContinue;
    DebuggerState m_debuggerState WTF_GUARDED_BY_LOCK(m_lock) { DebuggerState::Replied };
    MutatorState m_mutatorState WTF_GUARDED_BY_LOCK(m_lock) { MutatorState::Running };

    StopReason WTF_GUARDED_BY_LOCK(m_lock) m_stopReason;
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
