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

#if ENABLE(WEBASSEMBLY)

#include "WasmModule.h"
#include "WasmVirtualAddress.h"
#include "WeakGCMap.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSWebAssemblyModule;
class JSWebAssemblyInstance;
class VM;
class WebAssemblyGCStructure;

namespace Wasm {

class IPIntCallee;
class FunctionCodeIndex;

class ModuleManager {
    WTF_MAKE_TZONE_ALLOCATED(ModuleManager);

public:
    JS_EXPORT_PRIVATE ModuleManager(VM&);
    JS_EXPORT_PRIVATE ~ModuleManager();

    JS_EXPORT_PRIVATE uint32_t registerModule(Module&);
    JS_EXPORT_PRIVATE void unregisterModule(Module&);
    JS_EXPORT_PRIVATE Module* module(uint32_t moduleId) const;

    JS_EXPORT_PRIVATE uint32_t registerInstance(JSWebAssemblyInstance*);
    JS_EXPORT_PRIVATE JSWebAssemblyInstance* jsInstance(uint32_t instanceId) const;
    JS_EXPORT_PRIVATE uint32_t nextInstanceId() const;

    JS_EXPORT_PRIVATE String generateLibrariesXML() const;

    JS_EXPORT_PRIVATE String generateModuleName(VirtualAddress, const RefPtr<Module>&) const;

private:
    using IdToModule = UncheckedKeyHashMap<uint32_t, Module*, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using IdToInstance = WeakGCMap<uint32_t, JSWebAssemblyInstance, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    // No locks needed: mutator thread is suspended during debug operations, preventing concurrent access
    IdToModule m_moduleIdToModule;
    IdToInstance m_instanceIdToInstance;

    uint32_t m_nextModuleId { 0 };
    uint32_t m_nextInstanceId { 0 };
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
