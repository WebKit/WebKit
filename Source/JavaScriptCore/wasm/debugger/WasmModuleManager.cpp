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

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "VM.h"
#include "WasmDebugServerUtilities.h"
#include "WasmFormat.h"
#include "WasmInstanceAnchor.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
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

uint32_t ModuleManager::registerModule(Module& module)
{
    Locker locker { m_lock };
    uint32_t moduleId = m_nextModuleId++;
    m_moduleIdToModule.set(moduleId, &module);
    const auto& moduleInfo = module.moduleInformation();
    moduleInfo.debugInfo->id = moduleId;
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerModule] - registered module with ID: ", moduleId, " size: ", moduleInfo.debugInfo->source.size(), " bytes");
    return moduleId;
}

void ModuleManager::unregisterModule(Module& module)
{
    Locker locker { m_lock };
    uint32_t moduleId = module.debugId();
    m_moduleIdToModule.remove(moduleId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][unregisterModule] - unregistered module with debug ID: ", moduleId);
}

uint32_t ModuleManager::registerInstance(JSWebAssemblyInstance* jsInstance)
{
    Locker locker { m_lock };
    uint32_t instanceId = m_nextInstanceId++;

    RefPtr<InstanceAnchor> anchor = jsInstance->anchor();
    RELEASE_ASSERT(anchor, "Instance must have an anchor");

    amortizedCleanupIfNeeded();
    m_instanceIdToInstance.set(instanceId, ThreadSafeWeakPtr { *anchor });

    jsInstance->setDebugId(instanceId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerInstance] - registered instance with ID: ", instanceId, " for module ID: ", jsInstance->module().debugId());
    return instanceId;
}

Module* ModuleManager::module(uint32_t moduleId) const
{
    Locker locker { m_lock };
    auto itr = m_moduleIdToModule.find(moduleId);
    if (itr == m_moduleIdToModule.end()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][module] - module not found for ID: ", moduleId);
        return nullptr;
    }
    return itr->value;
}

JSWebAssemblyInstance* ModuleManager::jsInstance(uint32_t instanceId)
{
    Locker locker { m_lock };
    amortizedCleanupIfNeeded();

    auto it = m_instanceIdToInstance.find(instanceId);
    if (it == m_instanceIdToInstance.end()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][jsInstance] - instance not found for ID: ", instanceId);
        return nullptr;
    }

    // This function is called from the debugger thread (WasmDebugServer) to access JSWebAssemblyInstance
    // objects during LLDB packet processing. Using InstanceAnchor provides thread-safe access and automatic
    // cleanup when instances are destroyed.
    //
    // Safety guarantees:
    // 1. InstanceAnchor::tearDown() is called in JSWebAssemblyInstance destructor, nulling out the pointer
    // 2. VMs are stopped during debugger access per GDB remote protocol, preventing GC and ensuring stability
    // 3. The anchor's lock protects concurrent access to the instance pointer
    RefPtr<InstanceAnchor> anchor = it->value.get();
    if (!anchor) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][jsInstance] - anchor is dead for ID: ", instanceId);
        return nullptr;
    }

    Locker anchorLocker { anchor->m_lock };
    JSWebAssemblyInstance* instance = anchor->instance();
    if (!instance) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][jsInstance] - instance is null for ID: ", instanceId);
        return nullptr;
    }

    RELEASE_ASSERT(instance->vm().debugState()->isStopped(), "Instance exists but VM is not stopped");
    return instance;
}

static String generateModuleName(VirtualAddress address, const RefPtr<Module>&)
{
    // FIXME: Maybe we should generate a more meaningful name?
    String moduleName = WTF::makeString("wasm_module_0x"_s, address.hex(), ".wasm"_s);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateModuleName] Using fallback address-based name: ", moduleName);
    return moduleName;
}

String ModuleManager::generateLibrariesXML() const
{
    Locker locker { m_lock };
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

uint32_t ModuleManager::nextInstanceId() const
{
    Locker locker { m_lock };
    return m_nextInstanceId;
}

void ModuleManager::amortizedCleanupIfNeeded()
{
    if (++m_operationCountSinceLastCleanup > m_maxOperationCountWithoutCleanup) {
        m_instanceIdToInstance.removeIf([](auto& entry) {
            return !entry.value.get(); // Remove entries with dead anchors
        });
        cleanupHappened();
    }
}

void ModuleManager::cleanupHappened()
{
    m_operationCountSinceLastCleanup = 0;
    m_maxOperationCountWithoutCleanup = std::min(std::numeric_limits<unsigned>::max() / 2, static_cast<unsigned>(m_instanceIdToInstance.size())) * 2;
}

}
} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
