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
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModuleManager);

uint32_t ModuleManager::registerModule(Module& module)
{
    Locker locker { m_lock };
    uint32_t moduleId = m_nextModuleId++;
    const auto& moduleInfo = module.moduleInformation();
    moduleInfo.debugInfo->id = moduleId;
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerModule] - registered module with ID: ", moduleId, " size: ", moduleInfo.debugInfo->source.size(), " bytes");
    return moduleId;
}

void ModuleManager::unregisterModule(Module& module)
{
    Locker locker { m_lock };
    uint32_t moduleId = module.debugId();
    // A module outlives every instance of it, so all of its libraries are gone by now.
    if (m_moduleIdToPrimaryInstanceId.remove(moduleId))
        m_hasPendingLibraryRemovals = true;
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
    m_moduleIdToPrimaryInstanceId.add(jsInstance->module().debugId(), instanceId);
    m_unnotifiedInstanceIds.add(instanceId);

    jsInstance->setDebugId(instanceId);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][registerInstance] - registered instance with ID: ", instanceId, " for module ID: ", jsInstance->module().debugId());
    return instanceId;
}

bool ModuleManager::needsLibraryRequery() const
{
    Locker locker { m_lock };
    return !m_unnotifiedInstanceIds.isEmpty() || m_hasPendingLibraryRemovals;
}

void ModuleManager::notifyLibraryRequeryComplete()
{
    Locker locker { m_lock };
    m_unnotifiedInstanceIds.clear();
    m_hasPendingLibraryRemovals = false;
}

Vector<uint32_t> ModuleManager::unnotifiedInstanceIds() const
{
    Locker locker { m_lock };
    Vector<uint32_t> result;
    result.reserveInitialCapacity(m_unnotifiedInstanceIds.size());
    for (uint32_t id : m_unnotifiedInstanceIds)
        result.append(id);
    std::sort(result.begin(), result.end());
    return result;
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

    RELEASE_ASSERT(instance->vm().debugState()->isStopped, "Instance exists but VM is not stopped");
    return instance;
}

String ModuleManager::generateLibrariesXML() const
{
    Locker locker { m_lock };
    StringBuilder xml;
    xml.append("<?xml version=\"1.0\"?>\n"_s);
    xml.append("<library-list>\n"_s);

    auto appendXMLEscaped = [](StringBuilder& builder, const String& value) {
        for (UChar c : StringView(value).codeUnits()) {
            switch (c) {
            case '&':
                builder.append("&amp;"_s);
                break;
            case '<':
                builder.append("&lt;"_s);
                break;
            case '>':
                builder.append("&gt;"_s);
                break;
            case '"':
                builder.append("&quot;"_s);
                break;
            default:
                builder.append(c);
                break;
            }
        }
    };

    // Report libraries in instance-id order so the list LLDB sees is stable across requeries.
    Vector<std::pair<uint32_t, RefPtr<InstanceAnchor>>> liveInstances;
    liveInstances.reserveInitialCapacity(m_instanceIdToInstance.size());
    for (const auto& pair : m_instanceIdToInstance) {
        if (RefPtr<InstanceAnchor> anchor = pair.value.get())
            liveInstances.append({ pair.key, WTF::move(anchor) });
    }
    std::sort(liveInstances.begin(), liveInstances.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    unsigned libraryCount = 0;
    for (const auto& [instanceId, anchor] : liveInstances) {
        Locker anchorLocker { anchor->m_lock };
        JSWebAssemblyInstance* instance = anchor->instance();
        if (!instance)
            continue;

        const auto& debugInfo = instance->moduleInformation().debugInfo;
        if (debugInfo->source.isEmpty())
            continue;

        VirtualAddress moduleBaseAddress = VirtualAddress::createModule(instanceId);
        String libraryName = debugInfo->declaredName();
        auto primaryInstance = m_moduleIdToPrimaryInstanceId.find(debugInfo->id);
        if (libraryName.isEmpty()) {
            // No name to collide with: the base address already names the instance uniquely.
            libraryName = makeString("0x"_s, moduleBaseAddress.hex(), ".wasm"_s);
        } else if (primaryInstance == m_moduleIdToPrimaryInstanceId.end() || primaryInstance->value != instanceId)
            libraryName = makeString(libraryName, '@', instanceId);

        xml.append("  <library name=\""_s);
        appendXMLEscaped(xml, libraryName);
        xml.append("\">\n"_s);
        xml.append("    <section address=\"0x"_s);
        xml.append(moduleBaseAddress.hex());
        xml.append("\"/>\n"_s);
        xml.append("  </library>\n"_s);
        libraryCount++;
        dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateLibrariesXML] - added instance '", libraryName, "' ID: ", instanceId, " of module ID: ", debugInfo->id, " at ", moduleBaseAddress, " size: 0x", hex(debugInfo->source.size(), Lowercase));
    }

    xml.append("</library-list>\n"_s);

    String result = xml.toString();
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleManager][generateLibrariesXML] - generated library list XML: ", libraryCount, " instances, ", result.length(), " characters");
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
        bool removedAny = m_instanceIdToInstance.removeIf([](auto& entry) {
            return !entry.value.get(); // Remove entries with dead anchors
        });
        // A collected instance removes a library from the list; LLDB must be notified.
        if (removedAny)
            m_hasPendingLibraryRemovals = true;
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
