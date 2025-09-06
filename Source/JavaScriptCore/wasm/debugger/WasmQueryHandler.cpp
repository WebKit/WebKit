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

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "CallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "NativeCallee.h"
#include "Options.h"
#include "StackVisitor.h"
#include "VM.h"
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
    // For WASM32 architecture, provide WASM-specific register definitions
    StringView regNumStr = packet.substring(strlen("qRegisterInfo"));
    int regNum = static_cast<int>(parseHex(regNumStr));

    if (regNum == 0) {
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

    // Use packetSplit for consistent parsing
    auto parts = packetSplit(offsetPart, ","_s);
    if (parts.size() != 2)
        return false;

    offset = parseHex(parts[0]);
    maxSize = parseHex(parts[1]);
    return true;
}

bool QueryHandler::handleChunkedLibrariesResponse(size_t offset, size_t maxSize, String& response)
{
    String xmlData = m_debugServer.m_instanceManager->generateLibrariesXML();

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
    auto stopReason = m_debugServer.m_executionHandler->stopReason();
    RELEASE_ASSERT(stopReason.isValid() && stopReason.callFrame);
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] buildWasmCallStackResponse: starting manual stack walk from CallFrame ", RawPointer(stopReason.callFrame));

    Vector<VirtualAddress> frameAddresses;
    frameAddresses.append(stopReason.address);
    CallFrame* currentFrame = stopReason.callFrame;
    uint8_t* returnPC = nullptr;
    VirtualAddress virtualReturnPC;
    unsigned frameIndex = 0;

    while (getWasmReturnPC(currentFrame, returnPC, virtualReturnPC) && frameIndex < 100) {
        frameAddresses.append(virtualReturnPC);
        currentFrame = currentFrame->callerFrame();
        frameIndex++;
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] CallStack: finished walking call stack, processed ", frameIndex, " frames");

    StringBuilder result;
    for (VirtualAddress address : frameAddresses)
        result.append(toLittleEndianHex(address));
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] buildWasmCallStackResponse: collected ", frameAddresses.size(), " frames, response length: ", result.length());
    return result.toString();
}

void QueryHandler::handleStartNoAckMode()
{
    // Format: QStartNoAckMode
    // LLDB wants to disable ACK mode - acknowledge this
    // Reference: https://sourceware.org/gdb/onlinedocs/gdb/Packet-Acknowledgment.html
    // WebAssembly Context: ACK mode adds overhead to WASM debugging, so disabling improves performance
    // This is especially beneficial for WASM step-through debugging with many small packets

    m_debugServer.sendReplyOK(); // OK - WASM debugger supports no-ACK mode for better performance
    m_debugServer.m_noAckMode = true;
}

void QueryHandler::handleSupported()
{
    // Format: qSupported[:feature[;feature]...]
    // Query supported features and packet size
    // Reference: https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qSupported
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
    // Reference: https://github.com/llvm/llvm-project/blob/main/lldb/docs/lldb-gdb-remote.txt
    // LLDB is asking: Can we include thread list in stop replies
    // WebAssembly Context: WASM typically runs in single thread, so this is simple to support, we can easily include our single main thread in stop replies
    // Reply Decision: OK - WASM debugger will include thread info (just main thread) in stop replies

    m_debugServer.sendReplyOK();
}

void QueryHandler::handleEnableErrorStrings()
{
    // Format: QEnableErrorStrings
    // Reference: https://github.com/llvm/llvm-project/blob/main/lldb/docs/lldb-gdb-remote.txt
    // LLDB is asking: Enable error strings in replies for better debugging
    // WebAssembly Context: Error strings help debug WASM execution issues, useful for reporting WASM trap conditions and runtime errors
    // Reply Decision: OK - WASM debugger will include error strings in replies

    m_debugServer.sendReplyOK();
}

void QueryHandler::handleThreadStopInfo(StringView packet)
{
    // Format: qThreadStopInfo<thread-id>
    // Reference: https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qThreadStopInfo
    // LLDB is asking: Get stop info for specific thread (needed for frame variable)
    // WebAssembly Context: Provide stop reason for WASM thread
    // Reply Decision: Handled by execution handler
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Handling qThreadStopInfo for frame variable support");
    m_debugServer.m_executionHandler->handleThreadStopInfo(packet);
}

void QueryHandler::handleLibrariesRead(StringView packet)
{
    // Format: qXfer:libraries:read::<offset>,<length>
    // Transfer library list XML using simplified module manager
    // Reference: https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html#qXfer-library-list-read

    size_t offset, maxSize;
    if (!parseLibrariesReadPacket(packet, offset, maxSize)) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket); // Invalid format
        return;
    }

    String response;
    if (handleChunkedLibrariesResponse(offset, maxSize, response)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Sending library list chunk: offset=", offset, ", maxSize=", maxSize);
        m_debugServer.sendReply(response);
    } else {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to generate library list chunk");
        m_debugServer.sendErrorReply(ProtocolError::MemoryError); // Error generating response
    }
}

void QueryHandler::handleWasmCallStack(StringView packet)
{
    // Format: qWasmCallStack:<thread-id-in-hex>
    // Reference: LLDB WebAssembly debugging extension
    // LLDB is asking: Get WebAssembly call stack information for disassembly display
    // WebAssembly Context: This packet is essential for LLDB to show proper WASM disassembly
    // with source lines, instruction details, and frame information

    auto strings = packetSplit(packet, ":"_s);
    if (strings.size() != 2) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    StringView threadIdStr = strings[1];
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmCallStack thread ID: ", threadIdStr);

    uint64_t threadId = parseHex(threadIdStr);
    uint64_t mutatorThreadId = m_debugServer.mutatorThreadId();
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Parsed qWasmCallStack thread ID: ", threadId, ", mutator ID: ", RawPointer((void*)mutatorThreadId));

    RELEASE_ASSERT(threadId == mutatorThreadId);
    String response = buildWasmCallStackResponse();
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmCallStack response: ", response);
    m_debugServer.sendReply(response);
}

void QueryHandler::handleWasmLocal(StringView packet)
{
    // Format: qWasmLocal:<frame-index>;<variable-index>
    // Reference: LLDB WebAssembly debugging extension
    // LLDB is asking: Get value of WebAssembly local variable (function argument or local)
    // WebAssembly Context: Access function locals and parameters for debugging
    // Reply Decision: Return local value or address based on variable type

    auto parts = packetSplit(packet, ":;"_s);
    if (parts.size() != 3) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    uint32_t frameIndex = parseDecimal(parts[1]);
    uint32_t localIndex = parseDecimal(parts[2]);

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmLocal frame=", frameIndex, ", variable=", localIndex);

    // For now, only support frame 0 (current frame)
    if (frameIndex != 0) {
        m_debugServer.sendErrorReply(ProtocolError::UnknownCommand);
        return;
    }

    auto stopReason = m_debugServer.m_executionHandler->stopReason();
    auto functionIndex = stopReason.callee->functionIndex();
    const auto& moduleInfo = stopReason.instance->module().moduleInformation();
    const Vector<Type>& localTypes = moduleInfo.functions[functionIndex].localTypes;

    IPInt::IPIntLocal& local = stopReason.locals[localIndex];
    Type localType = localTypes[localIndex];
    logWasmLocalValue(localIndex, local, localType);

    uint64_t value = 0;
    String response;
    switch (localType.kind) {
    case TypeKind::I32: {
        uint64_t address = (uint64_t)local.i32;
        response = toLittleEndianHex(address);
        break;
    }
    default:
        RELEASE_ASSERT(false, "not supported");
        break;
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] qWasmLocal response: ", response, " value: ", RawPointer((void*)value));
    m_debugServer.sendReply(response);
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
