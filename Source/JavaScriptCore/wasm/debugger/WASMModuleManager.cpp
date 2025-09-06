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
#include "WasmModuleManager.h"

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "DeferGC.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "VM.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WeakGCMap.h"
#include "WeakGCMapInlines.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/IterationStatus.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModuleManager);

ModuleManager::ModuleManager(VM& vm)
    : m_vm(vm)
    , m_moduleIdToModule(vm)
    , m_instanceIdToInstance(vm)
{
}

ModuleManager::~ModuleManager() = default;

uint32_t ModuleManager::registerModule(JSWebAssemblyModule* jsModule)
{
    uint32_t moduleId = m_nextModuleId++;
    m_moduleIdToModule.set(moduleId, jsModule);
    const auto& moduleInfo = jsModule->moduleInformation();
    moduleInfo.debugInfo->setId(moduleId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][registerModule] - registered module with ID: ", moduleId, " size: ", moduleInfo.debugInfo->source().size(), " bytes");
    return moduleId;
}

uint32_t ModuleManager::registerInstance(JSWebAssemblyInstance* jsInstance)
{
    uint32_t instanceId = m_nextInstanceId++;
    m_instanceIdToInstance.set(instanceId, jsInstance);
    jsInstance->setDebugId(instanceId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][registerInstance] - registered instance with ID: ", instanceId, " for module ID: ", jsInstance->module().debugId());
    return instanceId;
}

JSWebAssemblyModule* ModuleManager::jsModule(uint32_t moduleId) const { return m_moduleIdToModule.get(moduleId); }
JSWebAssemblyInstance* ModuleManager::jsInstance(uint32_t instanceId) const { return m_instanceIdToInstance.get(instanceId); }

String ModuleManager::generateLibrariesXML() const
{
    // Generate XML library list for LLDB's GDB Remote Protocol
    StringBuilder xml;
    xml.append("<?xml version=\"1.0\"?>\n"_s);
    xml.append("<library-list>\n"_s);

    {
        DeferGC deferGC(m_vm);

        // Generate entries for each registered module
        m_moduleIdToModule.forEach([&](uint32_t moduleId, JSWebAssemblyModule* jsModule) {
            if (!jsModule)
                return IterationStatus::Continue;

            const auto& source = jsModule->moduleInformation().debugInfo->source();
            if (source.isEmpty())
                return IterationStatus::Continue;

            // Generate module base address using module ID
            VirtualAddress moduleBaseAddress = VirtualAddress::createModule(moduleId);

            // Generate module name
            String moduleName = generateModuleName(moduleBaseAddress, jsModule);
            xml.append("  <library name=\""_s);
            xml.append(moduleName);
            xml.append("\">\n"_s);
            xml.append("    <section address=\"0x"_s);
            xml.append(moduleBaseAddress.hex());
            xml.append("\"/>\n"_s);
            xml.append("  </library>\n"_s);
            dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][generateLibrariesXML] - added module '", moduleName, "' ID: ", moduleId, " at ", moduleBaseAddress, " size: 0x", hex(source.size(), Lowercase));
            return IterationStatus::Continue;
        });
    }

    xml.append("</library-list>\n"_s);

    String result = xml.toString();
    dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][generateLibrariesXML] - generated library list XML: ", m_moduleIdToModule.size(), " modules, ", result.length(), " characters");
    return result;
}

String ModuleManager::generateModuleName(VirtualAddress address, JSWebAssemblyModule* jsModule) const
{
    if (jsModule) {
        const auto& moduleInfo = jsModule->moduleInformation();
        // First priority: Use sourceMappingURL if available
        if (!moduleInfo.sourceMappingURL.isEmpty()) {
            // Convert Vector<char8_t> to String using WTF::makeString
            String url = WTF::makeString(moduleInfo.sourceMappingURL);

            // Extract filename from URL (handle both file paths and URLs)
            size_t lastSlash = url.reverseFind('/');
            size_t lastBackslash = url.reverseFind('\\');
            size_t lastSeparator = std::max(lastSlash == WTF::notFound ? 0 : lastSlash + 1,
                lastBackslash == WTF::notFound ? 0 : lastBackslash + 1);

            if (lastSeparator < url.length()) {
                String filename = url.substring(lastSeparator);
                // Ensure it has .wasm extension
                if (!filename.endsWithIgnoringASCIICase(".wasm"_s)) {
                    filename = WTF::makeString(filename, ".wasm"_s);
                }
                dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][generateModuleName] Using sourceMappingURL filename: ", filename);
                return filename;
            }
        }
    }

    // Final fallback: Use address-based naming
    String fallbackName = WTF::makeString("wasm_module_0x"_s, address.hex(), ".wasm"_s);
    dataLogLnIf(Options::verboseWasmDebugger(), "[InstanceManager][generateModuleName] Using fallback address-based name: ", fallbackName);
    return fallbackName;
}

size_t ModuleManager::instanceCount() const { return m_instanceIdToInstance.size(); }
size_t ModuleManager::moduleCount() const { return m_moduleIdToModule.size(); }
uint32_t ModuleManager::nextModuleId() const { return m_nextModuleId; }
uint32_t ModuleManager::nextInstanceId() const { return m_nextInstanceId; }

}
} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
