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
#include "WasmMemoryHandler.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "Options.h"
#include "WasmDebugServer.h"
#include "WasmDebugServerUtilities.h"
#include "WasmExecutionHandler.h"
#include "WasmModuleManager.h"
#include "WasmVirtualAddress.h"
#include <cstdlib>
#include <cstring>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MemoryHandler);

void MemoryHandler::read(StringView packet)
{
    // Format: m<addr>,<length>
    // LLDB: Read memory at specified address and length
    // Reference: [3] in wasm/debugger/README.md

    // WebAssembly Context: Read WASM module bytecode or memory data
    // Only allows access to WASM-specific virtual addresses for security
    if (packet.isEmpty() || packet[0] != 'm') {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    StringView params = packet.substring(1);
    auto parts = splitWithDelimiters(params, ","_s);
    if (parts.size() != 2) {
        m_debugServer.sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    VirtualAddress address = VirtualAddress(parseHex(parts[0]));
    size_t length = static_cast<size_t>(parseHex(parts[1]));

    StringBuilder data;
    VirtualAddress::Type addressType = address.type();

    bool success = false;
    if (addressType == VirtualAddress::Type::Module)
        success = readModuleData(address, length, data);
    else if (addressType == VirtualAddress::Type::Memory)
        success = readMemoryData(address, length, data);
    else
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] Rejecting non-WASM address for security: ", address);

    if (success)
        m_debugServer.sendReply(data.toString());
    else
        m_debugServer.sendErrorReply(ProtocolError::InvalidAddress);
}

bool MemoryHandler::readModuleData(VirtualAddress address, size_t length, StringBuilder& data)
{
    uint32_t id = address.id();
    uint32_t offset = address.offset();

    RefPtr module = m_debugServer.m_moduleManager->module(id);
    if (!module)
        return false;

    const auto& source = module->moduleInformation().debugInfo->source;
    if (!offset && source.size() < length) {
        // FIXME: This is a workaround for tiny modules - clamp initial read at offset 0.
        // Cannot clamp at non-zero offsets as it corrupts DWARF debug info in LLDB.
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - clamping read from ", length, " to ", source.size(), " bytes (module size: ", source.size(), ")");
        length = source.size();
    } else if (offset >= source.size() || offset + length > source.size()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - read beyond module boundary. Address: ", address, " offset: ", offset, " size: ", length, " module size: ", source.size());
        return false;
    }

    for (size_t i = 0; i < length; ++i)
        data.append(hex(source[offset + i], 2, Lowercase));

    dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - read ", length, " bytes at offset: ", offset, " from module ID: ", id);
    return true;
}

bool MemoryHandler::readMemoryData(VirtualAddress address, size_t length, StringBuilder& data)
{
    uint32_t instanceId = address.id();
    uint32_t offset = address.offset();

    JSWebAssemblyInstance* jsInstance = m_debugServer.m_moduleManager->jsInstance(instanceId);
    if (!jsInstance) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - instance not found for ID: ", instanceId);
        return false;
    }

    void* memoryBase = jsInstance->cachedMemory();
    size_t size = jsInstance->cachedMemorySize();
    if (!memoryBase || offset + length > size) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - memory access out of bounds. Instance ID: ", instanceId, " offset: ", offset, " size: ", length, " memory size: ", size);
        return false;
    }

    uint8_t* memPtr = static_cast<uint8_t*>(memoryBase) + offset;
    for (size_t i = 0; i < length; i++)
        data.append(hex(memPtr[i], 2, Lowercase));

    dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] - read ", length, " bytes at offset: ", offset, " from instance ID: ", instanceId);
    return true;
}

void MemoryHandler::handleMemoryRegionInfo(StringView packet)
{
    // Format: qMemoryRegionInfo:<addr>
    // LLDB: Get information about the memory region containing the specified address
    // Reference: [17] in wasm/debugger/README.md

    // WebAssembly Context: Provide memory region info for WASM modules, memory, stack, and globals
    // Return region info with start, size, permissions, and name
    StringView addressStr = packet.substring(strlen("qMemoryRegionInfo:"));
    if (addressStr.isEmpty()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] Malformed qMemoryRegionInfo packet");
        m_debugServer.sendErrorReply(ProtocolError::InvalidAddress);
        return;
    }

    VirtualAddress address = VirtualAddress(parseHex(addressStr));
    dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] qMemoryRegionInfo for address: ", address);

    VirtualAddress::Type addressType = address.type();
    uint32_t id = address.id();
    uint32_t offset = address.offset();

    dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] qMemoryRegionInfo: address=", address, ", type=", (int)addressType, ", id=", id, ", offset=0x", hex(offset, Lowercase));

    switch (addressType) {
    case VirtualAddress::Type::Memory:
        handleWasmMemoryRegionInfo(address, id, offset);
        break;
    case VirtualAddress::Type::Module:
        handleWasmModuleRegionInfo(address, id, offset);
        break;
    default:
        // Invalid address type - send error
        dataLogLnIf(Options::verboseWasmDebugger(), "[MemoryHandler] Invalid address type for memory region: ", (int)addressType);
        m_debugServer.sendErrorReply(ProtocolError::InvalidAddress);
        break;
    }
}

void MemoryHandler::handleWasmMemoryRegionInfo(VirtualAddress address, uint32_t instanceId, uint32_t offset)
{
    JSWebAssemblyInstance* instance = m_debugServer.m_moduleManager->jsInstance(instanceId);
    if (instance) {
        size_t memorySize = instance->cachedMemorySize();
        if (offset < memorySize) {
            // Address is within WASM memory - return the memory region
            uint32_t moduleId = instance->moduleInformation().debugInfo->id;
            String name = makeString("wasm_memory_"_s, instanceId, "_"_s, moduleId);
            sendMemoryRegionReply(address, memorySize, "rw"_s, name);
            return;
        }
    }

    uint32_t idUpperBoundary = m_debugServer.m_moduleManager->nextInstanceId();
    uint32_t nextValidID = instanceId;
    do {
        if (++nextValidID >= idUpperBoundary) {
            // No more instances - return unmapped region
            uint64_t unmappedSize = VirtualAddress::MODULE_BASE - address.value();
            sendUnmappedRegionReply(address, unmappedSize);
            return;
        }
    } while (!m_debugServer.m_moduleManager->jsInstance(nextValidID));

    // Address is beyond this module - return unmapped region to next module
    VirtualAddress nextMemoryAddress = VirtualAddress::createMemory(nextValidID);
    uint64_t unmappedSize = nextMemoryAddress.value() - address.value();
    sendUnmappedRegionReply(address, unmappedSize);
}

void MemoryHandler::handleWasmModuleRegionInfo(VirtualAddress address, uint32_t moduleId, uint32_t offset)
{
    JSWebAssemblyInstance* instance = m_debugServer.m_moduleManager->jsInstance(moduleId);
    if (instance) {
        JSWebAssemblyModule* jsModule = instance->jsModule();
        const auto& source = jsModule->moduleInformation().debugInfo->source;
        if (offset < source.size()) {
            // Address is within module bounds - return info for the entire WASM module region
            String name = makeString("wasm_module_"_s, moduleId);
            sendMemoryRegionReply(address, source.size(), "rx"_s, name, "module"_s);
            return;
        }
    }

    uint32_t idUpperBoundary = m_debugServer.m_moduleManager->nextInstanceId();
    uint32_t nextValidID = moduleId;
    do {
        if (++nextValidID >= idUpperBoundary) {
            // No more instances - return unmapped region to end of address space
            uint64_t unmappedSize = VirtualAddress::INVALID_END - address.value();
            sendUnmappedRegionReply(address, unmappedSize);
            return;
        }
    } while (!m_debugServer.m_moduleManager->jsInstance(nextValidID));

    // Address is beyond this module - return unmapped region to next module
    VirtualAddress nextModuleAddress = VirtualAddress::createModule(nextValidID);
    uint64_t unmappedSize = nextModuleAddress.value() - address.value();
    sendUnmappedRegionReply(address, unmappedSize);
}

void MemoryHandler::sendMemoryRegionReply(uint64_t start, uint64_t size, StringView permissions, StringView name)
{
    String response = makeString(
        "start:"_s, hex(start, Lowercase),
        ";size:"_s, hex(size, Lowercase),
        ";permissions:"_s, permissions,
        ";name:"_s, stringToHex(name),
        ";"_s);
    m_debugServer.sendReply(response);
}

void MemoryHandler::sendMemoryRegionReply(uint64_t start, uint64_t size, StringView permissions, StringView name, StringView type)
{
    String response = makeString(
        "start:"_s, hex(start, Lowercase),
        ";size:"_s, hex(size, Lowercase),
        ";permissions:"_s, permissions,
        ";name:"_s, stringToHex(name),
        ";type:"_s, type,
        ";"_s);
    m_debugServer.sendReply(response);
}

void MemoryHandler::sendUnmappedRegionReply(uint64_t start, uint64_t size)
{
    String response = makeString(
        "start:"_s, hex(start, Lowercase),
        ";size:"_s, hex(size, Lowercase),
        ";permissions:;name:;"_s);
    m_debugServer.sendReply(response);
}

NO_RETURN_DUE_TO_CRASH void MemoryHandler::write(StringView)
{
    RELEASE_ASSERT_NOT_REACHED("Not supported yet.");
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
