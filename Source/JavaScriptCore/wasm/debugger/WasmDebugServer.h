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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WasmDebugServerUtilities.h>
#include <JavaScriptCore/WasmGDBPacketParser.h>
#include <JavaScriptCore/WasmVirtualAddress.h>

#include <atomic>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <winsock2.h>
#endif

namespace JSC {

class VM;
class CallFrame;
class JSWebAssemblyInstance;
class JSWebAssemblyModule;

namespace IPInt {
struct IPIntLocal;
struct IPIntStackEntry;
}

namespace Wasm {

class QueryHandler;
class MemoryHandler;
class RegisterHandler;
class ExecutionHandler;
class IPIntCallee;
class Module;
class ModuleManager;
class BreakpointManager;

class DebugServer {
    WTF_MAKE_TZONE_ALLOCATED(DebugServer);

public:
    JS_EXPORT_PRIVATE static DebugServer& singleton();

#if OS(WINDOWS)
    using SocketType = SOCKET;
    static constexpr SocketType invalidSocketValue = INVALID_SOCKET;
#else
    using SocketType = int;
    static constexpr SocketType invalidSocketValue = -1;
#endif

    static constexpr uint16_t defaultPort = 1234;

    DebugServer();
    ~DebugServer() = default;

    JS_EXPORT_PRIVATE bool start();

#if ENABLE(REMOTE_INSPECTOR)
    // DebugServer supports two modes:
    // 1. Direct TCP socket mode (JSC shell debugging)
    // 2. Remote Web Inspector integration mode (WebKit debugging)
    bool isRWIMode() const { return !!m_rwiResponseHandler; }
    JS_EXPORT_PRIVATE void startRWI(Function<bool(const String&)>&& rwiResponseHandler);
#endif

    void trackInstance(JSWebAssemblyInstance*);
    void trackModule(Module&);
    void untrackModule(Module&);

    void setPort(uint64_t port) { m_port = port; }

    // Returns true when a GDB remote client is present at the transport layer — either a TCP
    // socket has been accepted or an RWI handler has been registered. This is a wire-level check
    // only: the debugger may not have completed its startup sequence yet.
    JS_EXPORT_PRIVATE bool NODELETE hasDebugger() const;

    // Non-blocking check: returns true once the debugger has sent its first 'c' (continue).
    // This is used for test only.
    bool hasContinued() const { return m_hasContinued.load(std::memory_order_acquire); }

    // True once the debugger has completed its startup exchange ('?' + first qXfer:libraries:read),
    // which is the point at which we consider the debugger fully ready to handle breakpoints, traps, and new module loads.
    bool isDebuggerReady() const
    {
        bool ready = m_isDebuggerReady.load(std::memory_order_acquire);
        RELEASE_ASSERT(!ready || hasDebugger());
        return ready;
    }

    JS_EXPORT_PRIVATE void handlePacket(StringView packet);

    ExecutionHandler& execution() const
    {
        RELEASE_ASSERT(m_executionHandler);
        return *m_executionHandler;
    }

    JS_EXPORT_PRIVATE ModuleManager& moduleManager() const;

    JS_EXPORT_PRIVATE void reset();

private:

    bool isInService() const { return m_isInService.load(std::memory_order_acquire); }
    void setIsInService() { m_isInService.store(true, std::memory_order_release); }

    bool createAndBindServerSocket();
    void startAcceptThread();
    void acceptClientConnections();
    void closeSocket(SocketType&);

    void handleClient();
    void handleThreadManagement(StringView packet);

    void sendAck();
    void sendReplyOK();
    void sendReplyNotSupported(StringView packet);
    void sendReply(StringView reply);
    void sendErrorReply(ProtocolError);

    bool isSocketValid(SocketType clientSocket) const
    {
#if OS(WINDOWS)
        return clientSocket != invalidSocketValue;
#else
        return clientSocket >= 0;
#endif
    }

    friend class QueryHandler;
    friend class MemoryHandler;
    friend class RegisterHandler;
    friend class ExecutionHandler;

    // Set once on start()/startRWI() and never cleared — DebugServer is a process-lifetime singleton.
    std::atomic<bool> m_isInService { false };
    std::atomic<bool> m_hasContinued { false };
    std::atomic<bool> m_isDebuggerReady { false };
    uint16_t m_port { defaultPort };
    SocketType m_serverSocket { invalidSocketValue };
    SocketType m_clientSocket { invalidSocketValue };
    RefPtr<Thread> m_acceptThread;

    bool m_noAckMode { false };
    std::unique_ptr<QueryHandler> m_queryHandler;
    std::unique_ptr<MemoryHandler> m_memoryHandler;
    std::unique_ptr<ExecutionHandler> m_executionHandler;

    std::unique_ptr<ModuleManager> m_moduleManager;

    GDBPacketParser m_packetParser;

#if ENABLE(REMOTE_INSPECTOR)
    Function<bool(const String&)> m_rwiResponseHandler;
#endif
};

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
