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
#include "WasmVirtualAddress.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "WasmCallee.h"
#include "WasmDebugServerUtilities.h"
#include "WasmFormat.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmModuleManager.h"
#include <wtf/DataLog.h>

namespace JSC {
namespace Wasm {

void VirtualAddress::dump(PrintStream& out) const
{
    Type addressType = this->type();
    uint32_t addressId = this->id();
    uint32_t addressOffset = this->offset();

    out.print("VirtualAddress(0x", WTF::hex(m_value, WTF::Lowercase), " -> ");

    switch (addressType) {
    case Type::Memory:
        out.print("Memory[instance:", addressId, ", offset:0x", WTF::hex(addressOffset, WTF::Lowercase), "])");
        break;
    case Type::Module:
        out.print("Module[module:", addressId, ", offset:0x", WTF::hex(addressOffset, WTF::Lowercase), "])");
        break;
    default:
        out.print("Unknown[type:", (int)addressType, ", id:", addressId, ", offset:0x", WTF::hex(addressOffset, WTF::Lowercase), "])");
        break;
    }
}

VirtualAddress VirtualAddress::toVirtual(JSWebAssemblyInstance* jsInstance, FunctionCodeIndex index, const uint8_t* pc)
{
    JSWebAssemblyModule* jsModule = jsInstance->jsModule();
    Ref module = jsModule->module();
    const Wasm::FunctionData& functionData = jsModule->moduleInformation().functions[index];
    uint32_t offset = static_cast<uint32_t>(pc - &functionData.data[0] + functionData.start);
    return VirtualAddress::createModule(module->debugId(), offset);
}

uint8_t* VirtualAddress::toPhysicalPC(const ModuleManager& moduleManager)
{
    RELEASE_ASSERT(type() == VirtualAddress::Type::Module);

    uint32_t id = this->id();
    uint32_t offset = this->offset();

    RefPtr module = moduleManager.module(id);
    if (!module || offset >= module->moduleInformation().debugInfo->source.size())
        return nullptr;

    const ModuleInformation& moduleInfo = module->moduleInformation();
    const auto& functions = moduleInfo.functions;

#if ASSERT_ENABLED
    for (size_t i = 1; i < functions.size(); ++i)
        ASSERT(functions[i - 1].start <= functions[i].start);
#endif

    auto it = std::upper_bound(functions.begin(), functions.end(), offset, [](uint32_t offset, const FunctionData& func) {
        return offset < func.start;
    });

    if (it != functions.begin()) {
        --it;
        const FunctionData& functionData = *it;
        if (offset >= functionData.start && offset < functionData.end) {
            uint32_t offsetInFunction = offset - functionData.start;
            uint8_t* pc = const_cast<uint8_t*>(&functionData.data[0]) + offsetInFunction;
            dataLogLnIf(Options::verboseWasmDebugger(), "Resolved virtual address: ", *this, " to physical PC: ", RawPointer(pc), " (function index: ", static_cast<size_t>(it - functions.begin()), ")");
            return pc;
        }
    }

    dataLogLnIf(Options::verboseWasmDebugger(), "Failed to resolve virtual address: ", *this, " - offset not found in any function");
    return nullptr;
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
