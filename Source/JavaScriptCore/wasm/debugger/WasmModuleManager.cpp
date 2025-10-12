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
    : m_instanceIdToInstance(vm)
{
}

ModuleManager::~ModuleManager() = default;

uint32_t ModuleManager::registerModule(Module& module)
{
    uint32_t moduleId = m_nextModuleId++;
    m_moduleIdToModule.set(moduleId, &module);
    const auto& moduleInfo = module.moduleInformation();
    moduleInfo.debugInfo->id = moduleId;
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerModule] - registered module with ID: ", moduleId, " size: ", moduleInfo.debugInfo->source.size(), " bytes");
    return moduleId;
}

void ModuleManager::unregisterModule(Module& module)
{
    uint32_t moduleId = module.debugId();
    m_moduleIdToModule.remove(moduleId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][unregisterModule] - unregistered module with debug ID: ", moduleId);
}

uint32_t ModuleManager::registerInstance(JSWebAssemblyInstance* jsInstance)
{
    uint32_t instanceId = m_nextInstanceId++;
    m_instanceIdToInstance.set(instanceId, jsInstance);
    jsInstance->setDebugId(instanceId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerInstance] - registered instance with ID: ", instanceId, " for module ID: ", jsInstance->module().debugId());
    return instanceId;
}

Module* ModuleManager::module(uint32_t moduleId) const
{
    auto itr = m_moduleIdToModule.find(moduleId);
    if (itr == m_moduleIdToModule.end()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][module] - module not found for ID: ", moduleId);
        return nullptr;
    }
    return itr->value;
}

JSWebAssemblyInstance* ModuleManager::jsInstance(uint32_t instanceId) const
{
    auto* result = m_instanceIdToInstance.get(instanceId);
    if (!result) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][jsInstance] - instance not found for ID: ", instanceId);
        return nullptr;
    }
    return result;
}

String ModuleManager::generateLibrariesXML() const
{
    StringBuilder xml;
    xml.append("<?xml version=\"1.0\"?>\n"_s);
    xml.append("<library-list>\n"_s);

    for (const auto& pair : m_moduleIdToModule) {
        uint32_t moduleId = pair.key;
        RefPtr module = pair.value;
        if (!module)
            continue;

        const auto& source = module->moduleInformation().debugInfo->source;
        if (source.isEmpty())
            continue;

        VirtualAddress moduleBaseAddress = VirtualAddress::createModule(moduleId);
        String moduleName = generateModuleName(moduleBaseAddress, module);
        xml.append("  <library name=\""_s);
        xml.append(moduleName);
        xml.append("\">\n"_s);
        xml.append("    <section address=\"0x"_s);
        xml.append(moduleBaseAddress.hex());
        xml.append("\"/>\n"_s);
        xml.append("  </library>\n"_s);
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateLibrariesXML] - added module '", moduleName, "' ID: ", moduleId, " at ", moduleBaseAddress, " size: 0x", hex(source.size(), Lowercase));
    }

    xml.append("</library-list>\n"_s);

    String result = xml.toString();
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateLibrariesXML] - generated library list XML: ", m_moduleIdToModule.size(), " modules, ", result.length(), " characters");
    return result;
}

String ModuleManager::generateModuleName(VirtualAddress address, const RefPtr<Module>&) const
{
    // FIXME: Maybe we should generate a more meaningful name?
    String fallbackName = WTF::makeString("wasm_module_0x"_s, address.hex(), ".wasm"_s);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateModuleName] Using fallback address-based name: ", fallbackName);
    return fallbackName;
}

uint32_t ModuleManager::nextInstanceId() const { return m_nextInstanceId; }

}
} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
