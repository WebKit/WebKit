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

#include "WasmDebugServerUtilities.h"
#include "WasmVirtualAddress.h"

#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
namespace Wasm {

class DebugServer;

class MemoryHandler {
    WTF_MAKE_TZONE_ALLOCATED(MemoryHandler);

public:
    MemoryHandler(DebugServer& debugServer)
        : m_debugServer(debugServer)
    {
    }

    void read(StringView packet);
    NO_RETURN_DUE_TO_CRASH void write(StringView packet);
    void handleMemoryRegionInfo(StringView packet);

private:
    DebugServer& m_debugServer;

    bool readModuleData(VirtualAddress, size_t length, StringBuilder& data);
    bool readMemoryData(VirtualAddress, size_t length, StringBuilder& data);

    void handleWasmMemoryRegionInfo(VirtualAddress, uint32_t instanceId, uint32_t offset);
    void handleWasmModuleRegionInfo(VirtualAddress, uint32_t moduleId, uint32_t offset);
    void sendMemoryRegionReply(uint64_t start, uint64_t size, StringView permissions, StringView name);
    void sendMemoryRegionReply(uint64_t start, uint64_t size, StringView permissions, StringView name, StringView type);
    void sendUnmappedRegionReply(uint64_t start, uint64_t size);
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
