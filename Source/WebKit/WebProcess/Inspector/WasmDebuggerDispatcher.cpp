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
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmDebuggerDispatcher.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER) && ENABLE(REMOTE_INSPECTOR)

#include "Connection.h"
#include "Logging.h"
#include "WasmDebuggerDispatcherMessages.h"
#include "WebProcess.h"
#include <JavaScriptCore/WasmDebugServer.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

WasmDebuggerDispatcher::WasmDebuggerDispatcher(WebProcess& process)
    : m_process(process)
    , m_queue(WorkQueue::create("com.apple.WebKit.WasmDebuggerDispatcher"_s))
{
}

WasmDebuggerDispatcher::~WasmDebuggerDispatcher()
{
    ASSERT_NOT_REACHED();
}

void WasmDebuggerDispatcher::ref() const
{
    m_process->ref();
}

void WasmDebuggerDispatcher::deref() const
{
    m_process->deref();
}

void WasmDebuggerDispatcher::initializeConnection(IPC::Connection& connection)
{
    // Register message receiver on WorkQueue (NOT main thread).
    // This allows IPC messages to be processed even when main thread is blocked in infinite loop
    connection.addMessageReceiver(m_queue.get(), *this, Messages::WasmDebuggerDispatcher::messageReceiverName());
}

void WasmDebuggerDispatcher::dispatchMessage(const String& message)
{
    // This method runs on WorkQueue thread (NOT main thread).
    // Safe to call even when main thread is blocked in infinite loop.
    JSC::Wasm::DebugServer& debugServer = JSC::Wasm::DebugServer::singleton();

    if (!debugServer.isConnected()) {
        RELEASE_LOG_ERROR(Inspector, "WasmDebugServer not connected");
        return;
    }

    debugServer.handlePacket(message);
}

} // namespace WebKit

#endif // ENABLE(WEBASSEMBLY_DEBUGGER) && ENABLE(REMOTE_INSPECTOR)
