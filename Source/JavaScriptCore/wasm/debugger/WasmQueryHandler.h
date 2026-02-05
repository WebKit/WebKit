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
#include "WasmVirtualAddress.h"

namespace JSC {
namespace Wasm {

class DebugServer;

class QueryHandler {
    WTF_MAKE_TZONE_ALLOCATED(QueryHandler);

public:
    QueryHandler(DebugServer& debugServer)
        : m_debugServer(debugServer)
    {
    }

    void handleGeneralQuery(StringView packet);

    void handleProcessInfo();
    void handleHostInfo();
    void handleRegisterInfo(StringView packet);

    void handleStartNoAckMode();
    void handleSupported();
    void handleListThreadsInStopReply();
    void handleEnableErrorStrings();
    void handleThreadStopInfo(StringView packet);
    void handleLibrariesRead(StringView packet);
    void handleWasmCallStack(StringView packet);
    void handleWasmLocal(StringView packet);

private:
    DebugServer& m_debugServer;

    bool parseLibrariesReadPacket(StringView packet, size_t& offset, size_t& maxSize);
    bool handleChunkedLibrariesResponse(size_t offset, size_t maxSize, String& response);
    String buildWasmCallStackResponse();
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
