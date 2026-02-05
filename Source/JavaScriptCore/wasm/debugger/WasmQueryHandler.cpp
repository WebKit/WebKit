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
#include "WasmQueryHandler.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "CallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "NativeCallee.h"
#include "Options.h"
#include "StackVisitor.h"
#include "VM.h"
#include "VMManager.h"
#include "WasmCallee.h"
#include "WasmDebugServer.h"
#include "WasmDebugServerUtilities.h"
#include "WasmExecutionHandler.h"
#include "WasmIPIntGenerator.h"
#include "WasmIPIntSlowPaths.h"
#include "WasmMemoryHandler.h"
#include "WasmModuleInformation.h"
#include "WasmModuleManager.h"
#include "WasmVirtualAddress.h"
#include <cstring>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(QueryHandler);

void QueryHandler::handleGeneralQuery(StringView packet)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling query: ", packet);

    if (packet.startsWith("QStartNoAckMode"_s))
        handleStartNoAckMode();
    else if (packet.startsWith("qSupported"_s))
        handleSupported();
    else if (packet.startsWith("QListThreadsInStopReply"_s))
        handleListThreadsInStopReply();
    else if (packet.startsWith("QEnableErrorStrings"_s))
        handleEnableErrorStrings();
    else if (packet.startsWith("qThreadStopInfo"_s))
        handleThreadStopInfo(packet);
    else if (packet.startsWith("qHostInfo"_s))
        handleHostInfo();
    else if (packet.startsWith("qProcessInfo"_s))
        handleProcessInfo();
    else if (packet.startsWith("qRegisterInfo"_s))
        handleRegisterInfo(packet);
    else if (packet.startsWith("qXfer:libraries:read::"_s))
        handleLibrariesRead(packet);
    else if (packet.startsWith("qWasmCallStack:"_s))
        handleWasmCallStack(packet);
    else if (packet.startsWith("qWasmLocal:"_s))
        handleWasmLocal(packet);
    else if (packet.startsWith("qMemoryRegionInfo:"_s))
        m_debugServer.m_memoryHandler->handleMemoryRegionInfo(packet);
    else
        m_debugServer.sendReplyNotSupported(packet);
}

void QueryHandler::handleProcessInfo()
{
    // Format: qProcessInfo
    // LLDB: Query process information for debugging context
    // Reference: [8] in wasm/debugger/README.md

    // WebAssembly Context: Provide WASM process info with simulated PID and WASI target
    // This helps LLDB understand the WebAssembly execution environment
    String tripleHex = stringToHex("wasm32-webkit-wasi"_s);
    String processInfo = makeString(
        "pid:1;"_s, // Process ID (simulated for WASM debugging)
        "parent-pid:1;"_s, // Parent process ID (simulated)
        "vendor:webkit;"_s, // WebKit/JavaScriptCore (identifies JSC's WASM debugger)
        "ostype:wasi;"_s, // WASI (WebAssembly System Interface)
        "arch:wasm32;"_s, // WebAssembly 32-bit architecture
        "triple:"_s, tripleHex, ";"_s, // Target triple: wasm32-webkit-wasi (hex encoded)
        "endian:little;"_s, // Little-endian byte order
        "ptrsize:4;"_s // 32-bit pointers
    );
    m_debugServer.sendReply(processInfo);
}

void QueryHandler::handleHostInfo()
{
    // Format: qHostInfo
    // LLDB: Query host system information for debugging setup
    // Reference: [9] in wasm/debugger/README.md

    // WebAssembly Context: Provide host info for WASM execution environment
    // This tells LLDB about the WebAssembly runtime characteristics
    String tripleHex = stringToHex("wasm32-webkit-wasi"_s);
    String hostInfo = makeString(
        "vendor:webkit;"_s, // WebKit/JavaScriptCore (identifies JSC's WASM debugger)
        "ostype:wasi;"_s, // WASI (WebAssembly System Interface)
        "arch:wasm32;"_s, // WebAssembly 32-bit architecture
        "triple:"_s, tripleHex, ";"_s, // Target triple: wasm32-webkit-wasi-wasm (hex encoded)
        "endian:little;"_s, // Little-endian byte order
        "ptrsize:4;"_s // 32-bit pointers
    );
    m_debugServer.sendReply(hostInfo);
}

void QueryHandler::handleRegisterInfo(StringView packet)
{
    // Format: qRegisterInfo<hex-reg-id>
    // LLDB: Query register information for specific register ID
    // Reference: [10] in wasm/debugger/README.md

    // WebAssembly Context: WASM only exposes PC register for debugging
    // Other registers are internal to the WASM runtime and not accessible
    StringView regNumStr = packet.substring(strlen("qRegisterInfo"));
    int regNum = static_cast<int>(parseHex(regNumStr));

    if (!regNum) {
        // PC register definition for WebAssembly debugging
        String registerInfo = makeString(
            "name:pc;"_s, // Program Counter register name
            "alt-name:pc;"_s, // Alternative name (same as primary)
            "bitsize:64;"_s, // 64-bit register size
            "offset:0;"_s, // Located at byte offset 0 in register context
            "encoding:uint;"_s, // Interpret contents as unsigned integer
            "format:hex;"_s, // Display in hexadecimal format by default
            "set:General Purpose Registers;"_s, // Belongs to GP register group
            "gcc:16;"_s, // GCC compiler register number
            "dwarf:16;"_s, // DWARF debug info register number
            "generic:pc;"_s // Generic register type (program counter)
        );
        m_debugServer.sendReply(registerInfo);
    } else {
        // Only PC register is supported - return error for all others
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Returning error for register ", regNum, " (only PC supported)");
        m_debugServer.sendErrorReply(ProtocolError::InvalidRegister);
    }
}

bool QueryHandler::parseLibrariesReadPacket(StringView packet, size_t& offset, size_t& maxSize)
{
    StringView offsetPart = packet.substring(strlen("qXfer:libraries:read::"));

    // Use splitWithDelimiters for consistent parsing
    auto parts = splitWithDelimiters(offsetPart, ","_s);
    if (parts.size() != 2)
        return false;

    offset = parseHex(parts[0]);
    maxSize = parseHex(parts[1]);
    return true;
}

bool QueryHandler::handleChunkedLibrariesResponse(size_t offset, size_t maxSize, String& response)
{
    String xmlData = m_debugServer.m_moduleManager->generateLibrariesXML();

    // Handle chunked response according to GDB Remote Protocol
    // 'm' prefix = more data follows
    // 'l' prefix = last chunk
    if (offset >= xmlData.length()) {
        response = "l"_s;
        return true;
    }

    size_t availableData = xmlData.length() - offset;
    size_t chunkSize = std::min(maxSize, availableData);

    String chunk = xmlData.substring(offset, chunkSize);
    bool isLastChunk = (offset + chunkSize >= xmlData.length());

    StringBuilder result;
    result.append(isLastChunk ? 'l' : 'm');
    result.append(chunk);
    response = result.toString();
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Chunked library response: ", (isLastChunk ? 'l' : 'm'), " offset=", offset, ", chunk_size=", chunkSize, ", total=", xmlData.length());
    return true;
}

String QueryHandler::buildWasmCallStackResponse()
{
    auto* state = m_debugServer.execution().debuggeeStateSafe();
    if (!state->atBreakpoint()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] buildWasmCallStackResponse: not stopped at breakpoint, returning empty");
        return String();
    }

    auto& stopData = *state->stopData;
    RELEASE_ASSERT(stopData.callFrame);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] buildWasmCallStackResponse: starting manual stack walk from CallFrame ", RawPointer(stopData.callFrame));

    Vector<VirtualAddress> frameAddresses;
    frameAddresses.append(stopData.address);
    CallFrame* currentFrame = stopData.callFrame;
    uint8_t* returnPC = nullptr;
    VirtualAddress virtualReturnPC;
    unsigned frameIndex = 0;

    // FIXME: Only supports consecutive wasm->wasm calls. Need to support interleaved wasm<->js calls (skipping stubs, including JS frames).
    while (getWasmReturnPC(currentFrame, returnPC, virtualReturnPC) && frameIndex < 100) {
        frameAddresses.append(virtualReturnPC);
        currentFrame = currentFrame->callerFrame();
        frameIndex++;
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] CallStack: finished walking call stack, processed ", frameIndex, " frames");

    StringBuilder result;
    for (VirtualAddress address : frameAddresses)
        result.append(toNativeEndianHex(address));
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] buildWasmCallStackResponse: collected ", frameAddresses.size(), " frames, response length: ", result.length());
    return result.toString();
}

void QueryHandler::handleStartNoAckMode()
{
    // Format: QStartNoAckMode
    // LLDB: Ask to disable ACK mode - acknowledge this
    // Reference: [6] in wasm/debugger/README.md

    // OK - WASM debugger supports no-ACK mode for better performance
    m_debugServer.sendReplyOK();
    m_debugServer.m_noAckMode = true;
}

void QueryHandler::handleSupported()
{
    // Format: qSupported[:feature[;feature]...]
    // LLDB: Query supported features and packet size
    // Reference: [7] in wasm/debugger/README.md

    // WebAssembly Context: We support qXfer:libraries:read+ to let LLDB discover WASM modules
    // This allows LLDB to see loaded WebAssembly modules as "libraries" for debugging
    String supportedFeatures = makeString(
        "qXfer:libraries:read+;"_s, // Support library list transfer for WASM modules
        "PacketSize=1000;"_s // Maximum packet size for data transfer
    );
    m_debugServer.sendReply(supportedFeatures);
}

void QueryHandler::handleListThreadsInStopReply()
{
    // Format: QListThreadsInStopReply
    // LLDB: Ask to include thread list in stop replies for better debugging
    // Reference: [11] in wasm/debugger/README.md

    // WebAssembly Context: WASM typically runs in single thread, so this is simple to support
    // We can easily include our single main thread in stop replies
    m_debugServer.sendReplyOK();
}

void QueryHandler::handleEnableErrorStrings()
{
    // Format: QEnableErrorStrings
    // LLDB: Enable error strings in replies for better debugging experience
    // Reference: [12] in wasm/debugger/README.md

    // WebAssembly Context: Error strings help debug WASM execution issues
    // Useful for reporting WASM trap conditions and runtime errors
    m_debugServer.sendReplyOK();
}

void QueryHandler::handleThreadStopInfo(StringView packet)
{
    // Format: qThreadStopInfo<thread-id>
    // LLDB: Get stop info for specific thread (needed for frame variable)
    // Reference: [13] in wasm/debugger/README.md

    // WebAssembly Context: Provide stop reason for WASM thread
    // Handled by execution handler for proper thread state management
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling qThreadStopInfo for frame variable support");
    m_debugServer.execution().handleThreadStopInfo(packet);
}

void QueryHandler::handleLibrariesRead(StringView packet)
{
    // Format: qXfer:libraries:read::<offset>,<length>
    // LLDB: Transfer library list XML for module discovery
    // Reference: [14] in wasm/debugger/README.md

    // WebAssembly Context: Provide WASM modules as "libraries" for LLDB
    // This allows LLDB to discover and debug loaded WebAssembly modules
    size_t offset, maxSize;
    if (!parseLibrariesReadPacket(packet, offset, maxSize)) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    String response;
    if (handleChunkedLibrariesResponse(offset, maxSize, response)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sending library list chunk: offset=", offset, ", maxSize=", maxSize);
        m_debugServer.sendReply(response);
    } else {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to generate library list chunk");
        m_debugServer.sendErrorReply(ProtocolError::MemoryError);
    }
}

void QueryHandler::handleWasmCallStack(StringView packet)
{
    // Format: qWasmCallStack:<thread-id-in-hex>
    // LLDB: Get WebAssembly call stack information for disassembly display
    // Reference: [15] in wasm/debugger/README.md

    // WebAssembly Context: This packet is essential for LLDB to show proper WASM disassembly
    // with source lines, instruction details, and frame information
    auto strings = splitWithDelimiters(packet, ":"_s);
    if (strings.size() != 2) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    StringView threadIdStr = strings[1];
    uint64_t requestedThreadId = parseHex(threadIdStr);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmCallStack thread ID: ", requestedThreadId);

    String response = m_debugServer.execution().callStackStringFor(requestedThreadId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmCallStack response: ", response);

    if (response.isEmpty()) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    m_debugServer.sendReply(response);
}

static constexpr uint32_t typeKindToWidth(TypeKind kind)
{
    switch (kind) {
#define CREATE_CASE(name, id, b3type, inc, wasmName, width, ...) \
    case TypeKind::name:                                         \
        return width;
        FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
    return 0;
}

void QueryHandler::handleWasmLocal(StringView packet)
{
    // Format: qWasmLocal:<frame-index>;<variable-index>
    // LLDB: Get value of WebAssembly local variable (function argument or local)
    // Reference: [16] in wasm/debugger/README.md

    // WebAssembly Context: Access function locals and parameters for debugging
    // Return local value or address based on variable type
    auto parts = splitWithDelimiters(packet, ":;"_s);
    if (parts.size() != 3) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    uint32_t frameIndex = parseDecimal(parts[1]);
    uint32_t localIndex = parseDecimal(parts[2]);

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmLocal frame=", frameIndex, ", variable=", localIndex);

    // For now, only support frame 0 (current frame)
    if (frameIndex) {
        m_debugServer.sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    auto* state = m_debugServer.execution().debuggeeStateSafe();
    if (!state->atBreakpoint()) {
        m_debugServer.sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    auto& stopData = *state->stopData;
    auto functionIndex = stopData.callee->functionIndex();
    const auto& moduleInfo = stopData.instance->module().moduleInformation();
    const Vector<Type>& localTypes = moduleInfo.debugInfo->ensureFunctionDebugInfo(functionIndex).locals;

    IPInt::IPIntLocal& local = stopData.locals[localIndex];
    Type localType = localTypes[localIndex];
    logWasmLocalValue(localIndex, local, localType);

    String response;
    uint32_t width = typeKindToWidth(localType.kind);
    switch (width) {
    case 32: {
        response = toNativeEndianHex(local.i32);
        break;
    }
    case 64: {
        response = toNativeEndianHex(local.i64);
        break;
    }
    case 128: {
        RELEASE_ASSERT(localType.kind == TypeKind::V128, "Expected V128 for 128-bit type");
        response = toNativeEndianHex(local.v128);
        break;
    }
    default:
        RELEASE_ASSERT(false, "Unsupported TypeKind bit size: ", width, " for TypeKind: ", localType.kind);
        break;
    }
    m_debugServer.sendReply(response);
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
